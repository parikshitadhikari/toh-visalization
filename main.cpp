#include "include/imports.h"
#include "include/define.h"

using namespace std;

struct Vector3
{
	double x, y, z;
	Vector3() { x = y = z = 0.0; }
	Vector3(double x, double y, double z) : x(x), y(y), z(z) {}
};

struct Disc
{
	Vector3 position; // represents position of the discs
};

struct ActiveDisc
{
	int disc_index;
	Vector3 start_pos, dest_pos; // initially 0,0,0 and 0,0,0
	double u;					 // u E [0, 1]
	double step_u;
	bool is_in_motion;
	int direction; // +1 for Left to Right & -1 for Right to left, 0 = stationary
};

struct Rod
{
	Vector3 positions[NUM_DISCS]; // vector3 positions1, positions2
	int occupancy_val[NUM_DISCS]; // occ_val1, occ_val2
};

struct GameBoard
{
	double x_min, y_min, x_max, y_max; // Base in XY-Plane
	double rod_base_rad;			   // Rod's base radius
	Rod rods[3];					   // Rod rod1, rod2, rod3
};

struct solution_pair
{
	size_t f, t; // f = from, t = to
};

// Game Globals
Disc discs[NUM_DISCS];
GameBoard t_board;
ActiveDisc active_disc;
list<solution_pair> sol; // linked list, stores sequence of 'solution_pair' structs, represents the solution steps
bool to_solve = false;

// Globals for window, time, FPS, moves
int moves = 0;
int prev_time = 0;
int window_width = WINDOW_WIDTH, window_height = WINDOW_HEIGHT;

// prototypes
void initialize();
void initialize_game();
void display_handler();
void reshape_handler(int w, int h);
void keyboard_handler(unsigned char key, int x, int y);
void anim_handler();
void move_disc(int from_rod, int to_rod);
Vector3 get_inerpolated_coordinate(Vector3 v1, Vector3 v2, double u);
void move_stack(int n, int f, int t);

// creates visualization of toh
int main2()
{
	// Initialize GLUT Window
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
	glutInitWindowSize(window_width, window_height);
	glutInitWindowPosition(0, 0);
	glutCreateWindow("Towers of Hanoi");
	glutDestroyWindow(1);
	initialize();
	cout << "TOH- Visualization" << endl;
	cout << "Press H for Help" << endl;

	// Callbacks
	glutDisplayFunc(display_handler);
	glutReshapeFunc(reshape_handler);
	glutKeyboardFunc(keyboard_handler);
	glutIdleFunc(anim_handler);

	glutMainLoop();
	return 0;
}

// setup initial configs and states
void initialize()
{
	glClearColor(0, 0, 0, 0);
	glEnable(GL_DEPTH_TEST); // Enabling Depth Test

	GLfloat light0_pos[] = {0.0f, 0.0f, 0.0f, 1.0f}; // w=1 represents pos. lighting
	// A positional light
	glLightfv(GL_LIGHT0, GL_POSITION, light0_pos);
	// Enabling Lighting
	glEnable(GL_LIGHTING);
	// Enabling Light0
	glEnable(GL_LIGHT0);
	// Initializing Game State
	initialize_game();
}

void initialize_game()
{
	// Initializing GameBoard
	t_board.rod_base_rad = 1;
	t_board.x_min = 0.0;
	t_board.x_max = BOARD_X * t_board.rod_base_rad;
	t_board.y_min = 0.0;
	t_board.y_max = BOARD_Y * t_board.rod_base_rad;

	double x_center = (t_board.x_max - t_board.x_min) / 2.0;
	double y_center = (t_board.y_max - t_board.y_min) / 2.0;
	// for single rod
	double dx = (t_board.x_max - t_board.x_min) / 3.0; // Since 3 rods
	double r = t_board.rod_base_rad;

	// Initializing Rods Occupancy value
	for (int i = 0; i < 3; i++)
	{
		for (int h = 0; h < NUM_DISCS; h++)
		{
			if (i == 0)
			{
				t_board.rods[i].occupancy_val[h] = NUM_DISCS - 1 - h;
			}
			else
				t_board.rods[i].occupancy_val[h] = -1;
		}
	}

	// Initializing positions of discs on rod
	for (int i = 0; i < 3; i++)
	{
		for (int h = 0; h < NUM_DISCS; h++)
		{
			double x = x_center + ((int)i - 1) * dx;
			double y = y_center;
			double z = (h + 1) * DISC_SPACING;
			Vector3 &pos_to_set = t_board.rods[i].positions[h]; // h=0, bottom disk
			pos_to_set.x = x;
			pos_to_set.y = y;
			pos_to_set.z = z;
		}
	}

	// Initializing Discs on first rod
	for (int i = 0; i < NUM_DISCS; i++)
	{
		discs[i].position = t_board.rods[0].positions[NUM_DISCS - i - 1];
	}

	// Initializing the property Active Disc
	active_disc.disc_index = -1;
	active_disc.is_in_motion = false;
	active_disc.step_u = 0.015; // determines how much the u parameter (used for animation) will be incremented in each step
	active_disc.u = 0.0;
	active_disc.direction = 0;
}

// Draw function for drawing a cylinder given position and radius and height
void draw_solid_cylinder(double x, double y, double r, double h)
{
	GLUquadric *q = gluNewQuadric();
	GLint slices = 50;
	GLint stacks = 2;
	glPushMatrix();
	glTranslatef(x, y, 0.0f);
	gluCylinder(q, r, r, h, slices, stacks);
	glTranslatef(0, 0, h);
	gluDisk(q, 0, r, slices, stacks);
	glPopMatrix();
	gluDeleteQuadric(q);
}

// Draw function for drawing rods on a given game board i.e. base
void draw_board_and_rods(GameBoard const &board)
{
	// Materials,
	GLfloat mat_white[] = {1.0f, 1.0f, 1.0f, 1.0f}; // board color
	GLfloat mat_red[] = {1.0f, 0.0f, 0.0f, 1.0f};	// rod color

	glPushMatrix();
	// Drawing the Base Rectangle [where the rods are placed]
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, mat_white);
	glBegin(GL_QUADS);
	glNormal3f(0.0f, 0.0f, 1.0f);
	glVertex2f(board.x_min, board.y_min);
	glVertex2f(board.x_min, board.y_max);
	glVertex2f(board.x_max, board.y_max);
	glVertex2f(board.x_max, board.y_min);
	glEnd();

	// Drawing Rods and Pedestals
	// Set material properties for rods and pedestals to yellow
	glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, mat_red);

	double r = board.rod_base_rad;
	for (int i = 0; i < 3; i++)
	{
		Vector3 const &p = board.rods[i].positions[0]; // position of first disc
		// Draw a solid cylinder representing the rod
		draw_solid_cylinder(p.x, p.y, r * 0.1, ROD_HEIGHT - 0.1);
		// Draw a solid cylinder representing the pedestal
		draw_solid_cylinder(p.x, p.y, r, 0.1);
	}
	glPopMatrix();
}

// Draw function for drawing discs
void draw_discs()
{
	int slices = 50;
	int stacks = 10;

	double rad;

	GLfloat r, g, b;
	GLfloat emission[] = {0.4f, 0.4f, 0.4f, 1.0f};
	GLfloat no_emission[] = {0.0f, 0.0f, 0.0f, 1.0f};
	GLfloat material[] = {1.0f, 1.0f, 1.0f, 1.0f};
	// deciding color for each disk
	for (size_t i = 0; i < NUM_DISCS; i++)
	{
		switch (i)
		{
		case 0: // color for first disk from top
			r = 0;
			g = 0;
			b = 1;
			break;
		case 1: // color for second disk from top
			r = 0;
			g = 1;
			b = 0;
			break;
		case 2:
			r = 0, g = 1;
			b = 1;
			break;
		case 3:
			r = 1;
			g = 0;
			b = 0;
			break;
		case 4:
			r = 1;
			g = 0;
			b = 1;
			break;
		case 5:
			r = 1;
			g = 1;
			b = 0;
			break;
		case 6:
			r = 1;
			g = 1;
			b = 1;
			break;
		default:
			r = g = b = 1.0f;
			break;
		};

		material[0] = r;
		material[1] = g;
		material[2] = b;
		// set material properties of each disk
		glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, material);

		GLfloat u = 0.0f;
		// highlight the disc in motion
		if (i == active_disc.disc_index)
		{
			glMaterialfv(GL_FRONT, GL_EMISSION, emission);
			u = active_disc.u;
		}

		GLfloat factor = 1.0f;
		switch (i)
		{
		case 0:
			factor = 0.2;
			break;
		case 1:
			factor = 0.4;
			break;
		case 2:
			factor = 0.6;
			break;
		case 3:
			factor = 0.8;
			break;
		case 4:
			factor = 1.2;
			break;
		case 5:
			factor = 1.4;
			break;
		case 6:
			factor = 1.6;
			break;
		case 7:
			factor = 1.8;
			break;
		default:
			break;
		};
		rad = factor * t_board.rod_base_rad; // radius of the rod
		int d = active_disc.direction;		 // direction of the rod

		glPushMatrix();
		glTranslatef(discs[i].position.x, discs[i].position.y, discs[i].position.z);
		glutSolidTorus(0.2 * t_board.rod_base_rad, rad, stacks, slices);
		glPopMatrix();

		glMaterialfv(GL_FRONT, GL_EMISSION, no_emission);
	}
}

// renders toh graphics on screen
void display_handler()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	double x_center = (t_board.x_max - t_board.x_min) / 2.0;
	double y_center = (t_board.y_max - t_board.y_min) / 2.0;
	double r = t_board.rod_base_rad;

	static float view[] = {0, 0, 0};
	view[0] = x_center;
	view[1] = y_center - 10;
	view[2] = 3 * r;

	glMatrixMode(GL_MODELVIEW); // sets the current matrix mode to the mvm
	glLoadIdentity();
	gluLookAt(view[0], view[1], view[2], // position
			  x_center, y_center, 3.0,	 // target
			  0.0, 0.0, 1.0);			 // up direction

	glPushMatrix(); // push camera matrix
	// then draw the game elements:
	draw_board_and_rods(t_board);
	draw_discs();
	glPopMatrix();
	glFlush();
	glutSwapBuffers();
}

void reshape_handler(int w, int h)
{
	window_width = w;
	window_height = h;
	glViewport(0, 0, (GLsizei)w, (GLsizei)h);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(45.0, (GLfloat)w / (GLfloat)h, 0.1, 20.0);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}

void move_stack(int n, int f, int t)
{
	if (n == 1)
	{
		solution_pair s;
		s.f = f;
		s.t = t;
		sol.push_back(s); // pushing the (from, to) pair of solution to a list [so that it can be animated later]
		moves++;
		cout << "From rod " << f << " to Rod " << t << endl;
		return;
	}
	move_stack(n - 1, f, 3 - t - f);
	move_stack(1, f, t);
	move_stack(n - 1, 3 - t - f, t);
}

// Solve from 1st rod to 2nd
void solve()
{
	move_stack(NUM_DISCS, 0, 2);
}

void keyboard_handler(unsigned char key, int x, int y)
{
	switch (key)
	{
	case 27:
	case 'q':
	case 'Q':
		exit(0);
		break;

	case 'h':
	case 'H':
		cout << "ESC: Quit" << endl;
		cout << "S: Solve from Initial State" << endl;
		cout << "H: Help" << endl;
		break;

	case 's':
	case 'S':
		solve();
		to_solve = true;
		break;
	default:
		break;
	};
}

void reshape(int w, int h)
{
	glViewport(0, 0, w, h);
	glMatrixMode(GL_PROJECTION); // switch to projection matrix
	glLoadIdentity();			 // resets projection matrix
	gluOrtho2D(0, w, h, 0);		 // 2d mode
	glMatrixMode(GL_MODELVIEW);	 // switches back to mvm
	glLoadIdentity();
}
void drawStrokeText(const char *text, int x, int y, int z)
{
	const char *c;
	glPushMatrix();
	glTranslatef(x, y, z);
	glScalef(0.5f, -0.5f, z);

	for (c = text; *c != '\0'; c++)
	{
		glutStrokeCharacter(GLUT_STROKE_ROMAN, *c);
	}
	glPopMatrix();
}

void render(void)
{
	glClear(GL_COLOR_BUFFER_BIT); // cls
	glLoadIdentity();			  // resets mvm to identity matrix

	glColor3f(0, 0, 1);
	drawStrokeText("TOH - VISUALIZATION", 400, 200, 0);
	glColor3f(1, 1, 0);
	drawStrokeText("- Parikshit Adhikari (077BCT054)", 100, 300, 0);
	drawStrokeText("- Prayag Raj Acharya (077BCT061)", 100, 400, 0);
	glColor3f(1, 0, 0);
	drawStrokeText("Press N to go to the next screen", 100, 600, 0);

	glutSwapBuffers();
}

void keyboard_handler_for_intro(unsigned char key, int x, int y)
{
	switch (key)
	{
	case 27:
	case 'q':
	case 'Q':
		exit(0);
		break;

	case 'h':
	case 'H':
		cout << "ESC: Quit" << endl;
		cout << "S: Solve from Initial State" << endl;
		cout << "H: Help" << endl;
		break;

	case 'n':
	case 'N':
		main2();

	default:
		break;
	};
}
int main(int argc, char *argv[])
{
	glutInit(&argc, argv);
	// specify the display mode to be RGB and double buffering
	glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE);

	glutInitWindowSize(1350, 690);
	glutInitWindowPosition(0, 0);
	glutCreateWindow("Towers Of Hanoi");
	// callbacks:
	glutKeyboardFunc(keyboard_handler_for_intro);
	glutDisplayFunc(render);
	glutReshapeFunc(reshape);

	glutMainLoop();
	return 0;
}

// initiates movement of a disk from one rod to another
void move_disc(int from_rod, int to_rod)
{
	int d = to_rod - from_rod;

	if (d > 0)
		active_disc.direction = 1;
	else if (d < 0)
		active_disc.direction = -1;
	// checking for invalid moves
	if ((from_rod == to_rod) || (from_rod < 0) || (to_rod < 0) || (from_rod > 2) || (to_rod > 2))
		return;

	int i;
	// finds the topmost disc, if top disc is empy slot, function returns without performing any further actions
	for (i = NUM_DISCS - 1; i >= 0 && t_board.rods[from_rod].occupancy_val[i] < 0; i--)
		;
	if ((i < 0) || (i == 0 && t_board.rods[from_rod].occupancy_val[i] < 0))
		return;
	// Either the index < 0 or index at 0 and occupancy < 0 => it's an empty rod

	active_disc.start_pos = t_board.rods[from_rod].positions[i];
	active_disc.disc_index = t_board.rods[from_rod].occupancy_val[i];
	active_disc.is_in_motion = true;
	active_disc.u = 0.0;

	int j;
	// stops when it finds the first empty slot (negative occupancy value).
	for (j = 0; j < NUM_DISCS - 1 && t_board.rods[to_rod].occupancy_val[j] >= 0; j++)
		;
	active_disc.dest_pos = t_board.rods[to_rod].positions[j];

	t_board.rods[from_rod].occupancy_val[i] = -1;
	t_board.rods[to_rod].occupancy_val[j] = active_disc.disc_index;
}

// calculates interpolated coordinate between two given points using Hermite interpolations
Vector3 get_inerpolated_coordinate(Vector3 sp, Vector3 tp, double u)
{
	// 4 Control points
	Vector3 p;
	double x_center = (t_board.x_max - t_board.x_min) / 2.0;
	double y_center = (t_board.y_max - t_board.y_min) / 2.0;

	double u3 = u * u * u;
	double u2 = u * u;

	Vector3 cps[4]; // P1, P2, dP1, dP2

	// Hermite Interpolation
	// Check Reference for equation of spline
	{
		// P1
		cps[0].x = sp.x;
		cps[0].y = y_center;
		cps[0].z = ROD_HEIGHT + 0.2 * (t_board.rod_base_rad);

		// P2
		cps[1].x = tp.x;
		cps[1].y = y_center;
		cps[1].z = ROD_HEIGHT + 0.2 * (t_board.rod_base_rad);

		// dP1
		cps[2].x = (sp.x + tp.x) / 2.0 - sp.x;
		cps[2].y = y_center;
		cps[2].z = 2 * cps[1].z; // change 2 * ..

		// dP2
		cps[3].x = tp.x - (tp.x + sp.x) / 2.0;
		cps[3].y = y_center;
		cps[3].z = -cps[2].z; //- cps[2].z;

		double h0 = 2 * u3 - 3 * u2 + 1;
		double h1 = -2 * u3 + 3 * u2;
		double h2 = u3 - 2 * u2 + u;
		double h3 = u3 - u2;

		p.x = h0 * cps[0].x + h1 * cps[1].x + h2 * cps[2].x + h3 * cps[3].x;
		p.y = h0 * cps[0].y + h1 * cps[1].y + h2 * cps[2].y + h3 * cps[3].y;
		p.z = h0 * cps[0].z + h1 * cps[1].z + h2 * cps[2].z + h3 * cps[3].z;
	}

	return p;
}

Vector3 operator-(Vector3 const &v1, Vector3 const &v2)
{
	return Vector3(v1.x - v2.x, v1.y - v2.y, v1.z - v2.z);
}

void anim_handler()
{
	int FPS = 60;
	int curr_time = glutGet(GLUT_ELAPSED_TIME); // time in ms since program started
	int elapsed = curr_time - prev_time;		// in ms, time elapsed since last frame
	if (elapsed < 1000 / FPS)					// 1k/fps: time that each frame should ideally take to achieve desired F.R.
		return;

	prev_time = curr_time;

	if (to_solve && active_disc.is_in_motion == false)
	{
		solution_pair s = sol.front();

		cout << s.f << ", " << s.t << endl;

		sol.pop_front();
		int i;
		for (i = NUM_DISCS - 1; i >= 0 && t_board.rods[s.f].occupancy_val[i] < 0; i--)
			; // finds index of topmost disk
		int ind = t_board.rods[s.f].occupancy_val[i];

		if (ind >= 0)
			active_disc.disc_index = ind;

		move_disc(s.f, s.t);
		if (sol.size() == 0)
			to_solve = false;
	}

	if (active_disc.is_in_motion)
	{
		int ind = active_disc.disc_index;
		ActiveDisc &ad = active_disc;

		if (ad.u == 0.0 && (discs[ind].position.z < ROD_HEIGHT))
		{
			discs[ind].position.z += 0.05; // actually moves the disc closer to viewer
			glutPostRedisplay();
			return;
		}
		static bool done = false;
		if (ad.u == 1.0 && discs[ind].position.z > ad.dest_pos.z)
		{
			done = true;
			discs[ind].position.z -= 0.05;
			glutPostRedisplay();
			return;
		}
		ad.u += ad.step_u;
		if (ad.u > 1.0)
		{
			ad.u = 1.0;
		}
		if (!done)
		{
			Vector3 prev_p = discs[ind].position;
			Vector3 p = get_inerpolated_coordinate(ad.start_pos, ad.dest_pos, ad.u);
			discs[ind].position = p;
		}
		if (ad.u >= 1.0 && discs[ind].position.z <= ad.dest_pos.z)
		{
			discs[ind].position.z = ad.dest_pos.z;
			ad.is_in_motion = false;
			done = false;
			ad.u = 0.0;
			ad.disc_index = -1;
		}
		glutPostRedisplay();
	}
}
