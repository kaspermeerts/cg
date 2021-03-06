#ifndef CG_SCENE
#define CG_SCENE

#include "cgmath.h"
#include "colour.h"
#include "texture.h"
#include "mesh.h"
#include "lighting.h"

enum { MAX_LIGHTS=8 };

typedef struct Camera {
	Vec3 position;
	Vec3 u, v, w; /* For the raytracer */
	Quaternion orientation; /* For the rasteriser */
	float fov;
	float near_plane;
	char *name;
} Camera;

typedef struct Disk {
	float radius;
} Disk;

typedef struct Sphere {
	float radius;
} Sphere;

typedef struct Cylinder {
	float radius;
	float height;
	bool capped;
} Cylinder;

typedef struct Cone {
	float radius;
	float height;
	bool capped;
} Cone;

typedef struct Torus {
	float inner_radius;
	float outer_radius;
} Torus;

typedef struct Shape {
	enum { SHAPE_PLANE, SHAPE_DISK, SHAPE_SPHERE, SHAPE_CYLINDER, SHAPE_CONE,
			SHAPE_MESH } type;
	union {
		Plane plane;
		Disk disk;
		Sphere sphere;
		Cylinder cylinder;
		Cone cone;
		Mesh *mesh;
	} u;
	char *name;
} Shape;

typedef struct Surface {
	Shape *shape;
	Material *material;
	Mat4 model_to_world;
	Mat4 world_to_model;
	BBox bbox;
	struct Surface *next;
} Surface;

typedef struct Scene {
	Camera *camera;
	int num_lights;
	Light *light[MAX_LIGHTS];
	Colour background;
	CubeMap *environment_map;
	Surface *root;
} Scene;

typedef struct Sdl {
	/* Resources */
	int num_cameras;
	Camera *camera;
	int num_lights;
	Light *light;
	int num_shapes;
	Shape *shape;
	int num_textures;
	Texture *texture;
	int num_materials;
	Material *material;

	Scene internal_scene;
} Sdl;

typedef struct Config {
	int width;
	int height;
	bool antialiasing;
	int aa_samples;
	int shadow_samples;
	int reflection_samples;
	int max_reflections;
	bool depth_of_field;
} Config;

const Config *config;
const Scene *scene;

Sdl *sdl_load(const char *filename);

#endif
