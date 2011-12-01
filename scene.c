#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include "scene.h"
#include "mesh.h"

static Config internal_config;

static Vec3 parse_vec3(const char *string)
{
	assert(string != NULL);
	Vec3 v;
	char *end;

	v.x = strtof(string, &end);
	v.y = strtof(end, &end);
	v.z = strtof(end, &end);

	return v;
}

static double parse_double(const char *string)
{
	assert(string != NULL);
	char *end;

	return strtod(string, &end);
}

static int parse_int(const char *string)
{
	assert(string != NULL);
	char *end;

	return strtol(string, &end, 10);
}

static Colour parse_colour(const char *string)
{
	assert(string != NULL);
	Colour c;
	char *end;

	c.r = strtof(string, &end);
	c.g = strtof(end, &end);
	c.b = strtof(end, &end);

	return c;
}

static bool parse_bool(const char *string)
{
	assert(string != NULL);
	return (strcmp(string, "true") == 0) || (strcmp(string, "1") == 0);
}

static char *strdup(const char *string)
{
	assert(string != NULL);
	char *new;
	size_t len;

	len = strlen(string);
	new = malloc(len+1);
	memcpy(new, string, len);
	new[len] = '\0';

	return new;
}

static bool import_config(xmlNode *node)
{
	internal_config.width = parse_int(xmlGetProp(node, "width"));
	internal_config.height = parse_int(xmlGetProp(node, "height"));
	internal_config.antialiasing = parse_bool(xmlGetProp(node, "antialiasing"));

	config = &internal_config;
	return true;
}

static bool import_cameras(Sdl *sdl, xmlNode *node, int n)
{
	int i;
	xmlNode *cur_node;
	Vec3 direction, up;
	Mat3 m;

	sdl->num_cameras = n;
	sdl->camera = calloc(n, sizeof(Camera));

	for (i = 0, cur_node = xmlFirstElementChild(node); cur_node; 
			i++, cur_node = xmlNextElementSibling(cur_node))
	{
		Camera *cam = &sdl->camera[i];
		assert(strcmp(cur_node->name, "Camera") == 0);

		cam->position = parse_vec3(xmlGetProp(cur_node, "position"));
		cam->fov = parse_double(xmlGetProp(cur_node, "fovy"));
		cam->name = strdup(xmlGetProp(cur_node, "name"));

		direction = parse_vec3(xmlGetProp(cur_node, "direction"));
		up = parse_vec3(xmlGetProp(cur_node, "up"));
		cam->w = vec3_scale(-1, vec3_normalize(direction));
		cam->u = vec3_normalize(vec3_cross(up, cam->w));
		cam->v = vec3_cross(cam->w, cam->u);
#define M(i, j) m[3*j + i]
		M(0, 0) = cam->u.x; M(0, 1) = cam->v.x; M(0, 2) = cam->w.x;
		M(1, 0) = cam->u.y; M(1, 1) = cam->v.y; M(1, 2) = cam->w.y;
		M(2, 0) = cam->u.z; M(2, 1) = cam->v.z; M(2, 2) = cam->w.z;
#undef M
		cam->orientation = quat_from_mat3(m);
	}
	assert(i == n);

	return true;
}

static bool import_lights(Sdl *sdl, xmlNode *node, int n)
{
	int i;
	xmlNode *cur_node;

	sdl->num_lights = n;
	sdl->light = calloc(n, sizeof(Light));

	for (i = 0, cur_node = xmlFirstElementChild(node); cur_node;
			i++, cur_node = xmlNextElementSibling(cur_node))
	{
		Light *light = &sdl->light[i];

		if (strcmp(cur_node->name , "DirectionalLight") == 0)
		{
			light->type = LIGHT_DIRECTIONAL;
			light->direction = vec3_normalize(
					parse_vec3(xmlGetProp(cur_node, "direction")));
		} else if (strcmp(cur_node->name, "PointLight") == 0)
		{
			light->type = LIGHT_POINT;
			light->position = parse_vec3(xmlGetProp(cur_node, "position"));
		} else if (strcmp(cur_node->name, "SpotLight") == 0)
		{
			light->type = LIGHT_SPOT;
			light->position = parse_vec3(xmlGetProp(cur_node, "position"));
			light->direction = vec3_normalize(
					parse_vec3(xmlGetProp(cur_node, "direction")));
			light->angle = parse_double(xmlGetProp(cur_node, "angle"));
		} else
		{
			printf("Unknown light type: \"%s\"\n", cur_node->name);
			return false;
		}

		light->colour = parse_colour(xmlGetProp(cur_node, "color"));
		light->intensity = parse_double(xmlGetProp(cur_node, "intensity"));
		light->name = strdup(xmlGetProp(cur_node, "name"));
	}
	assert(i == n);

	return true;
}

static bool import_shapes(Sdl *sdl, xmlNode *node, int n)
{
	int i;
	xmlNode *cur_node;

	sdl->num_shapes = n;
	sdl->shape = calloc(n, sizeof(Shape));

	for (i = 0, cur_node = xmlFirstElementChild(node); cur_node;
			i++, cur_node = xmlNextElementSibling(cur_node))
	{
		Shape *shape = &sdl->shape[i];
		if (strcmp(cur_node->name, "Sphere") == 0)
		{
			shape->type = SHAPE_SPHERE;
			shape->u.sphere.radius =
					parse_double(xmlGetProp(cur_node, "radius"));
		} else if (strcmp(cur_node->name, "Cylinder") == 0)
		{
			shape->type = SHAPE_CYLINDER;
			shape->u.cylinder.radius =
					parse_double(xmlGetProp(cur_node, "radius"));
			shape->u.cylinder.height =
					parse_double(xmlGetProp(cur_node, "height"));
			shape->u.cylinder.capped =
					parse_bool(xmlGetProp(cur_node, "capped"));
		} else if (strcmp(cur_node->name, "Cone") == 0)
		{
			shape->type = SHAPE_CONE;
			shape->u.cone.radius = parse_double(xmlGetProp(cur_node, "radius"));
			shape->u.cone.height = parse_double(xmlGetProp(cur_node, "height"));
			shape->u.cone.capped = parse_bool(xmlGetProp(cur_node, "capped"));
		} else if (strcmp(cur_node->name, "Torus") == 0)
		{
			shape->type = SHAPE_TORUS;
			shape->u.torus.inner_radius =
					parse_double(xmlGetProp(cur_node, "innerRadius"));
			shape->u.torus.outer_radius =
					parse_double(xmlGetProp(cur_node, "outerRadius"));
		} else if (strcmp(cur_node->name, "Mesh") == 0)
		{
			const char *filename;
			shape->type = SHAPE_MESH;
			filename = xmlGetProp(cur_node, "src");
			shape->u.mesh = mesh_load(filename);
			if (shape->u.mesh == NULL)
			{
				printf("Couldn't load mesh %s\n", filename);
				return false;
			}
		} else
		{
			printf("Unknown geometry type: %s\n", cur_node->name);
			return false;
		}
		shape->name = strdup(xmlGetProp(cur_node, "name"));

	}
	assert(i == n);
	return true;
}

static bool import_textures(Sdl *sdl, xmlNode *node, int n)
{
	int i;
	xmlNode *cur_node;

	sdl->num_textures = n;
	sdl->texture = calloc(n, sizeof(Texture));

	for (i = 0, cur_node = xmlFirstElementChild(node); cur_node;
			i++, cur_node = xmlNextElementSibling(cur_node))
	{
		Texture *tex = &sdl->texture[i];
		assert(strcmp(cur_node->name, "Texture") == 0);
		tex->source = strdup(xmlGetProp(cur_node, "src"));
		tex->name = strdup(xmlGetProp(cur_node, "name"));
		assert("Texture should be loaded here" == NULL);
		/* TODO: Load texture here? */
	}
	assert(i == n);

	return true;
}

static bool import_materials(Sdl *sdl, xmlNode *node, int n)
{
	int i;
	xmlNode *cur_node;

	sdl->num_materials = n;
	sdl->material = calloc(n, sizeof(Material));

	for (i = 0, cur_node = xmlFirstElementChild(node); cur_node;
			i++, cur_node = xmlNextElementSibling(cur_node))
	{
		Material *mat = &sdl->material[i];
		mat->diffuse_colour =
				parse_colour(xmlGetProp(cur_node, "diffuse_color"));
		mat->specular_colour =
				parse_colour(xmlGetProp(cur_node, "specular_color"));
		mat->shininess =
				parse_double(xmlGetProp(cur_node, "specular_exponent"));

		mat->name = strdup(xmlGetProp(cur_node, "name"));
	}

	return true;
}

static bool import_light_refs(Sdl *sdl, const char *light_names)
{
	int i;
	const char *name, *end;
	Scene *rw_scene = &sdl->internal_scene;

	if (*light_names == '\0')
	{
		rw_scene->num_lights = 0;
		return true;
	}

	i = 1;
	for (name = light_names; *name; name++)
		if (*name == ',')
			i++;
	rw_scene->num_lights = i;

	if (rw_scene->num_lights > MAX_LIGHTS)
	{
		printf("Too many lights: %d\n", rw_scene->num_lights);
		return false;
	}

	i = 0;
	name = end = light_names;
	while (*end != '\0')
	{
		end++;
		if (*end == ',' || *end == '\0')
		{
			rw_scene->light[i] = NULL;
			for (int j = 0; j < sdl->num_lights; j++)
				if (strncmp(sdl->light[j].name, name, end - name) == 0)
					rw_scene->light[i] = &sdl->light[j];
			if (rw_scene->light[i] == NULL)
			{
				printf("Couldn't find light %d of %s\n", i, light_names);
				return false;
			}
			name = end + 1;
			i++;
		}
	}

	return true;
}

static bool import_graph(Sdl *sdl, Surface **root, xmlNode *xml_node,
		MatrixStack *stack)
{
	xmlNode *child_node;

	if (strcmp(xml_node->name, "Shape") == 0)
	{
		const char *shape_name, *material_name;
		Surface *surf;
		surf = calloc(1, sizeof(Surface));
		surf->next = *root;
		*root = surf;
		shape_name = xmlGetProp(xml_node, "geometry");
		surf->shape = NULL;
		for (int i = 0; i < sdl->num_shapes; i++)
			if (strcmp(sdl->shape[i].name, shape_name) == 0)
				surf->shape = &sdl->shape[i];
		if (surf->shape == NULL)
		{
			printf("Requested shape \"%s\" not found\n", shape_name);
			return false;
		}
		if (xmlHasProp(xml_node, "texture"))
		{
			printf("Sorry, no texture support yet\n");
			return false;
		}
		material_name = xmlGetProp(xml_node, "material");
		surf->material = NULL;
		for (int i = 0; i < sdl->num_materials; i++)
			if (strcmp(sdl->material[i].name, material_name) == 0)
				surf->material = &sdl->material[i];
		if (surf->material == NULL)
		{
			printf("Requested material \"%s\" not found\n", material_name);
			return false;
		}
		mat4_copy(surf->model_to_world, stack->top->matrix);
		mat4_copy(surf->world_to_model, stack->top->inverse);
	} else
	{
		Mat4 mat, inv;

		if (strcmp(xml_node->name, "Rotate") == 0)
		{
			double angle;
			Vec3 axis;

			angle = parse_double(xmlGetProp(xml_node, "angle")) *
					M_TWO_PI / 360.;
			axis = parse_vec3(xmlGetProp(xml_node, "axis"));

			mat4_rotate(mat,  angle, axis.x, axis.y, axis.z);
			mat4_rotate(inv, -angle, axis.x, axis.y, axis.z);
		} else if (strcmp(xml_node->name, "Translate") == 0)
		{
			Vec3 v;

			v = parse_vec3(xmlGetProp(xml_node, "vector"));

			mat4_translate_vector(mat, v);
			mat4_translate_vector(inv, vec3_scale(-1, v));
		} else if (strcmp(xml_node->name, "Scale") == 0)
		{
			Vec3 v;

			v = parse_vec3(xmlGetProp(xml_node, "scale"));
			mat4_scale(mat, v.x, v.y, v.z);
			mat4_scale(inv, 1./v.x, 1./v.y, 1./v.z);
		} else
		{
			printf("Unknown node: \"%s\"\n", xml_node->name);
			return false;
		}

		matstack_push(stack);

		mat4_rmul(stack->top->matrix, mat);
		mat4_lmul(inv, stack->top->inverse);
		child_node = xmlFirstElementChild(xml_node);
		while (child_node)
		{
			if (!import_graph(sdl, root, child_node, stack))
				return false;
			child_node = xmlNextElementSibling(child_node);
		}

		matstack_pop(stack);
	}
	return true;
}

static bool import_scene(Sdl *sdl, xmlNode *node, int n)
{
	Scene *rw_scene = &sdl->internal_scene;
	int i;
	const char *cam_name, *light_names;
	MatrixStack *model_matrix;

	n = n; /* UNUSED */

	/* Camera */
	if (!xmlHasProp(node, "camera"))
	{
		printf("At least one camera has to be defined\n");
		return false;
	}
	cam_name = xmlGetProp(node, "camera");
	rw_scene->camera = NULL;
	for (i = 0; i < sdl->num_cameras; i++)
		if (strcmp(sdl->camera[i].name, cam_name) == 0)
			rw_scene->camera = &sdl->camera[i];

	if (rw_scene->camera == NULL)
	{
		printf("Requested camera \"%s\" not found\n", cam_name);
		return false;
	}

	/* Light(s) */
	if (!xmlHasProp(node, "lights"))
	{
		printf("A scene without lights is pretty dark...\n");
		return false;
	}
	light_names = xmlGetProp(node, "lights");
	if (!import_light_refs(sdl, light_names))
		return false;

	/* Background */
	rw_scene->background = parse_colour(xmlGetProp(node, "background"));

	/* The actual scene */
	model_matrix = matstack_new();
	matstack_push(model_matrix);
	mat4_identity(model_matrix->top->matrix);
	mat4_identity(model_matrix->top->inverse);

	rw_scene->root = NULL;
	if (!import_graph(sdl, &rw_scene->root, xmlFirstElementChild(node),
			model_matrix))
	{
		printf("Error importing the scene graph\n");
		matstack_destroy(model_matrix);
		return false;
	}
	matstack_destroy(model_matrix);

	scene = &sdl->internal_scene;
	return true;
}

static bool import_sdl(Sdl *sdl, xmlDoc *doc)
{
	xmlNode *root, *node;

	root = xmlDocGetRootElement(doc);
	if (root == NULL)
	{
		printf("No root element\n");
		return false;
	}

	for (node = xmlFirstElementChild(root); node != NULL;
			node = xmlNextElementSibling(node))
	{
		int n;
		assert(node->type == XML_ELEMENT_NODE);

		n = xmlChildElementCount(node);

		if (strcmp(node->name, "Config") == 0)
		{
			if (!import_config(node))
				return false;
		}
		else if (strcmp(node->name, "Cameras") == 0)
		{
			if (!import_cameras(sdl, node, n))
				return false;
		} else if (strcmp(node->name, "Lights") == 0)
		{
			if (!import_lights(sdl, node, n))
				return false;
		} else if (strcmp(node->name, "Geometry") == 0)
		{
			if (!import_shapes(sdl, node, n))
				return false;
		} else if (strcmp(node->name, "Textures") == 0)
		{
			if (!import_textures(sdl, node, n))
				return false;
		} else if (strcmp(node->name, "Materials") == 0)
		{
			if (!import_materials(sdl, node, n))
				return false;
		} else if (strcmp(node->name, "Scene") == 0)
		{
			if (!import_scene(sdl, node, n))
				return false;
		} else
		{
			printf("Unknown node: %s\n", node->name);
			return false;
		}
	}

	return true;
}

Sdl *sdl_load(const char *filename)
{
	Sdl *sdl = NULL;
	xmlDoc *doc = NULL;

	sdl = malloc(sizeof(Sdl));

	LIBXML_TEST_VERSION

	doc = xmlReadFile(filename, NULL, XML_PARSE_DTDLOAD | XML_PARSE_DTDVALID);
	if (doc == NULL)
	{
		printf("Error opening file %s\n", filename);
		goto errorout;
	}

	if (!import_sdl(sdl, doc))
	{
		printf("Error importing SDL from file %s\n", filename);
		goto errorout;
	}

	xmlFreeDoc(doc);
	xmlCleanupParser();
	return sdl;
errorout:
	free(sdl);
	if (doc) xmlFreeDoc(doc);
	xmlCleanupParser();
	return NULL;
}
