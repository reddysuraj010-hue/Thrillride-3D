#include <GL/freeglut.h>
#include <GL/glu.h>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace
{
	constexpr float Pi = 3.14159265358979323846f;
	constexpr int WindowWidth = 1280;
	constexpr int WindowHeight = 800;
	constexpr int TrackSamples = 720;
	constexpr float TrackHalfWidth = 0.72f;

	struct Vec3
	{
		float x;
		float y;
		float z;

		Vec3 operator+(const Vec3& other) const { return { x + other.x, y + other.y, z + other.z }; }
		Vec3 operator-(const Vec3& other) const { return { x - other.x, y - other.y, z - other.z }; }
		Vec3 operator*(float scale) const { return { x * scale, y * scale, z * scale }; }
	};

	struct TrackPoint
	{
		Vec3 position;
		Vec3 tangent;
	};

	std::vector<Vec3> controlPoints = {
		{ -13.0f, 4.0f, -8.0f }, { -7.0f, 9.5f, -13.0f }, { 1.0f, 12.0f, -12.0f },
		{ 9.0f, 7.0f, -11.0f }, { 13.0f, 4.0f, -5.0f }, { 11.0f, 5.0f, 3.0f },
		{ 7.0f, 10.0f, 9.0f }, { 1.0f, 13.0f, 11.0f }, { -6.0f, 8.0f, 12.0f },
		{ -12.0f, 5.0f, 8.0f }, { -14.0f, 4.0f, 1.0f }
	};
	std::vector<TrackPoint> track;
	float rideProgress = 0.0f;
	float rideSpeed = 0.16f;
	float orbitYaw = 35.0f;
	float orbitPitch = 22.0f;
	float orbitDistance = 36.0f;
	int cameraMode = 0;
	int windowWidth = WindowWidth;
	int windowHeight = WindowHeight;
	int previousMouseX = 0;
	int previousMouseY = 0;
	bool dragging = false;
	bool paused = false;
	bool lightingEnabled = true;
	bool showHud = true;
	GLUquadric* quadric = nullptr;

	Vec3 normalized(const Vec3& value)
	{
		const float length = std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
		if (length < 0.0001f)
		{
			return { 0.0f, 0.0f, 1.0f };
		}
		return value * (1.0f / length);
	}

	Vec3 catmullRom(const Vec3& p0, const Vec3& p1, const Vec3& p2, const Vec3& p3, float t)
	{
		const float t2 = t * t;
		const float t3 = t2 * t;
		return (p1 * 2.0f + (p2 - p0) * t +
			(p0 * 2.0f - p1 * 5.0f + p2 * 4.0f - p3) * t2 +
			(p0 * -1.0f + p1 * 3.0f - p2 * 3.0f + p3) * t3) * 0.5f;
	}

	void buildTrack()
	{
		track.clear();
		const int pointCount = static_cast<int>(controlPoints.size());
		for (int sample = 0; sample < TrackSamples; ++sample)
		{
			const float scaled = static_cast<float>(sample) * pointCount / TrackSamples;
			const int segment = static_cast<int>(scaled);
			const float t = scaled - segment;
			const Vec3& p0 = controlPoints[(segment - 1 + pointCount) % pointCount];
			const Vec3& p1 = controlPoints[segment % pointCount];
			const Vec3& p2 = controlPoints[(segment + 1) % pointCount];
			const Vec3& p3 = controlPoints[(segment + 2) % pointCount];
			const Vec3 position = catmullRom(p0, p1, p2, p3, t);
			const Vec3 tangent = normalized(catmullRom(p0, p1, p2, p3, t + 0.001f) -
				catmullRom(p0, p1, p2, p3, t - 0.001f));
			track.push_back({ position, tangent });
		}
	}

	const TrackPoint& trackAt(float progress)
	{
		int index = static_cast<int>(progress * track.size()) % static_cast<int>(track.size());
		if (index < 0)
		{
			index += static_cast<int>(track.size());
		}
		return track[static_cast<std::size_t>(index)];
	}

	void setMaterial(float red, float green, float blue, float shine = 24.0f)
	{
		const GLfloat color[] = { red, green, blue, 1.0f };
		const GLfloat specular[] = { 0.48f, 0.48f, 0.48f, 1.0f };
		glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, color);
		glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, specular);
		glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, shine);
		glColor3f(red, green, blue);
	}

	void drawGround()
	{
		glDisable(GL_LIGHTING);
		glColor3f(0.20f, 0.40f, 0.25f);
		glBegin(GL_QUADS);
		glVertex3f(-90.0f, -0.15f, -90.0f);
		glVertex3f(90.0f, -0.15f, -90.0f);
		glVertex3f(90.0f, -0.15f, 90.0f);
		glVertex3f(-90.0f, -0.15f, 90.0f);
		glEnd();

		glColor3f(0.25f, 0.47f, 0.29f);
		glBegin(GL_LINES);
		for (int i = -90; i <= 90; i += 5)
		{
			glVertex3f(static_cast<float>(i), -0.13f, -90.0f);
			glVertex3f(static_cast<float>(i), -0.13f, 90.0f);
			glVertex3f(-90.0f, -0.13f, static_cast<float>(i));
			glVertex3f(90.0f, -0.13f, static_cast<float>(i));
		}
		glEnd();
		glEnable(GL_LIGHTING);
	}

	void drawSupports()
	{
		setMaterial(0.25f, 0.29f, 0.34f, 45.0f);
		for (std::size_t i = 0; i < track.size(); i += 36)
		{
			const Vec3& point = track[i].position;
			const float baseY = 0.0f;
			const float height = std::max(0.0f, point.y - 0.55f - baseY);
			glPushMatrix();
			glTranslatef(point.x, baseY, point.z);
			glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
			gluCylinder(quadric, 0.13, 0.10, height, 10, 1);
			glPopMatrix();

			const Vec3 side = normalized({ -track[i].tangent.z, 0.0f, track[i].tangent.x });
			for (float direction : { -1.0f, 1.0f })
			{
				const Vec3 foot = point + side * (direction * 1.05f);
				glPushMatrix();
				glTranslatef(foot.x, 0.04f, foot.z);
				glScalef(0.25f, 0.12f, 0.25f);
				glutSolidCube(1.0);
				glPopMatrix();
			}
		}
	}

	void drawTrack()
	{
		glDisable(GL_LIGHTING);
		glLineWidth(7.0f);
		for (float side : { -TrackHalfWidth, TrackHalfWidth })
		{
			glColor3f(0.80f, 0.84f, 0.90f);
			glBegin(GL_LINE_STRIP);
			for (const TrackPoint& point : track)
			{
				const Vec3 lateral = normalized({ -point.tangent.z, 0.0f, point.tangent.x });
				const Vec3 rail = point.position + lateral * side;
				glVertex3f(rail.x, rail.y, rail.z);
			}
			const TrackPoint& first = track.front();
			const Vec3 lateral = normalized({ -first.tangent.z, 0.0f, first.tangent.x });
			const Vec3 rail = first.position + lateral * side;
			glVertex3f(rail.x, rail.y, rail.z);
			glEnd();
		}

		glLineWidth(1.0f);
		glEnable(GL_LIGHTING);
		setMaterial(0.17f, 0.19f, 0.23f, 10.0f);
		for (std::size_t i = 0; i < track.size(); i += 9)
		{
			const TrackPoint& point = track[i];
			const Vec3 lateral = normalized({ -point.tangent.z, 0.0f, point.tangent.x });
			const Vec3 start = point.position - lateral * (TrackHalfWidth + 0.15f);
			const Vec3 end = point.position + lateral * (TrackHalfWidth + 0.15f);
			const Vec3 middle = (start + end) * 0.5f;
			const float angle = std::atan2(lateral.z, lateral.x) * 180.0f / Pi;
			glPushMatrix();
			glTranslatef(middle.x, middle.y - 0.16f, middle.z);
			glRotatef(angle, 0.0f, 1.0f, 0.0f);
			glScalef(0.18f, 0.12f, 1.0f);
			glutSolidCube(1.0);
			glPopMatrix();
		}
	}

	void drawCart(const TrackPoint& point)
	{
		const float heading = std::atan2(point.tangent.x, point.tangent.z) * 180.0f / Pi;
		glPushMatrix();
		glTranslatef(point.position.x, point.position.y + 0.38f, point.position.z);
		glRotatef(heading, 0.0f, 1.0f, 0.0f);

		setMaterial(0.84f, 0.12f, 0.08f, 60.0f);
		glPushMatrix();
		glScalef(1.9f, 0.42f, 1.12f);
		glutSolidCube(1.0);
		glPopMatrix();

		setMaterial(0.95f, 0.72f, 0.10f, 35.0f);
		for (float seatZ : { -0.29f, 0.29f })
		{
			glPushMatrix();
			glTranslatef(0.0f, 0.34f, seatZ);
			glScalef(1.14f, 0.48f, 0.44f);
			glutSolidCube(1.0);
			glPopMatrix();
		}

		setMaterial(0.12f, 0.14f, 0.18f, 48.0f);
		for (float x : { -0.62f, 0.62f })
		{
			for (float z : { -0.55f, 0.55f })
			{
				glPushMatrix();
				glTranslatef(x, -0.22f, z);
				glutSolidTorus(0.07, 0.16, 8, 14);
				glPopMatrix();
			}
		}
		glPopMatrix();
	}

	void drawTree(float x, float z, float scale)
	{
		glPushMatrix();
		glTranslatef(x, 0.0f, z);
		glScalef(scale, scale, scale);
		setMaterial(0.34f, 0.20f, 0.10f, 4.0f);
		glPushMatrix();
		glTranslatef(0.0f, 1.4f, 0.0f);
		glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
		gluCylinder(quadric, 0.22, 0.14, 2.8, 8, 1);
		glPopMatrix();
		setMaterial(0.12f, 0.40f, 0.19f, 8.0f);
		glPushMatrix();
		glTranslatef(0.0f, 3.2f, 0.0f);
		glutSolidSphere(1.1, 12, 10);
		glTranslatef(0.0f, 0.75f, 0.0f);
		glutSolidSphere(0.75, 12, 10);
		glPopMatrix();
		glPopMatrix();
	}

	void drawPark()
	{
		for (int i = 0; i < 18; ++i)
		{
			const float angle = 2.0f * Pi * i / 18.0f;
			const float radius = 23.0f + 2.0f * std::sin(i * 2.3f);
			drawTree(std::cos(angle) * radius, std::sin(angle) * radius, 0.8f + 0.22f * (i % 3));
		}

		setMaterial(0.22f, 0.27f, 0.33f, 18.0f);
		glPushMatrix();
		glTranslatef(-19.0f, 1.2f, -17.0f);
		glScalef(5.2f, 2.4f, 3.4f);
		glutSolidCube(1.0);
		glPopMatrix();
		setMaterial(0.92f, 0.48f, 0.10f, 32.0f);
		glPushMatrix();
		glTranslatef(-19.0f, 2.5f, -17.0f);
		glScalef(5.5f, 0.18f, 3.7f);
		glutSolidCube(1.0);
		glPopMatrix();
	}

	void drawString(float x, float y, const std::string& text, void* font = GLUT_BITMAP_HELVETICA_18)
	{
		glRasterPos2f(x, y);
		for (unsigned char character : text)
		{
			glutBitmapCharacter(font, character);
		}
	}

	void drawHud()
	{
		if (!showHud)
		{
			return;
		}

		glMatrixMode(GL_PROJECTION);
		glPushMatrix();
		glLoadIdentity();
		gluOrtho2D(0.0, static_cast<double>(windowWidth), 0.0, static_cast<double>(windowHeight));
		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
		glLoadIdentity();
		glDisable(GL_LIGHTING);
		glDisable(GL_DEPTH_TEST);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glColor4f(0.035f, 0.055f, 0.09f, 0.82f);
		glBegin(GL_QUADS);
		glVertex2f(22.0f, static_cast<float>(windowHeight - 166));
		glVertex2f(410.0f, static_cast<float>(windowHeight - 166));
		glVertex2f(410.0f, static_cast<float>(windowHeight - 22));
		glVertex2f(22.0f, static_cast<float>(windowHeight - 22));
		glEnd();

		glColor3f(1.0f, 0.72f, 0.12f);
		drawString(42.0f, static_cast<float>(windowHeight - 54), "THRILLRIDE 3D", GLUT_BITMAP_HELVETICA_18);
		glColor3f(0.88f, 0.92f, 0.98f);
		drawString(42.0f, static_cast<float>(windowHeight - 83), "Interactive roller coaster simulation");
		std::ostringstream status;
		status << (paused ? "PAUSED" : "RUNNING") << "   Speed " << std::fixed << std::setprecision(2) << rideSpeed;
		drawString(42.0f, static_cast<float>(windowHeight - 111), status.str());
		const char* cameraName = cameraMode == 0 ? "Orbit" : (cameraMode == 1 ? "Chase" : "Front seat");
		drawString(42.0f, static_cast<float>(windowHeight - 139), std::string("Camera: ") + cameraName + "   [C]");

		glColor3f(0.95f, 0.96f, 1.0f);
		drawString(26.0f, 30.0f, "SPACE pause   +/- speed   C camera   L lights   H HUD   R reset   ESC quit");
		glDisable(GL_BLEND);
		glEnable(GL_DEPTH_TEST);
		if (lightingEnabled)
		{
			glEnable(GL_LIGHTING);
		}
		glPopMatrix();
		glMatrixMode(GL_PROJECTION);
		glPopMatrix();
		glMatrixMode(GL_MODELVIEW);
	}

	void applyCamera()
	{
		const TrackPoint& cart = trackAt(rideProgress);
		if (cameraMode == 1)
		{
			const Vec3 eye = cart.position - cart.tangent * 7.0f + Vec3{ 0.0f, 3.4f, 0.0f };
			const Vec3 target = cart.position + Vec3{ 0.0f, 0.6f, 0.0f };
			gluLookAt(eye.x, eye.y, eye.z, target.x, target.y, target.z, 0.0, 1.0, 0.0);
			return;
		}
		if (cameraMode == 2)
		{
			const Vec3 eye = cart.position + Vec3{ 0.0f, 1.15f, 0.0f };
			const Vec3 target = eye + cart.tangent * 9.0f + Vec3{ 0.0f, 0.8f, 0.0f };
			gluLookAt(eye.x, eye.y, eye.z, target.x, target.y, target.z, 0.0, 1.0, 0.0);
			return;
		}

		const float yaw = orbitYaw * Pi / 180.0f;
		const float pitch = orbitPitch * Pi / 180.0f;
		const Vec3 target{ 0.0f, 5.2f, 0.0f };
		const Vec3 eye{
			target.x + orbitDistance * std::cos(pitch) * std::sin(yaw),
			target.y + orbitDistance * std::sin(pitch),
			target.z + orbitDistance * std::cos(pitch) * std::cos(yaw)
		};
		gluLookAt(eye.x, eye.y, eye.z, target.x, target.y, target.z, 0.0, 1.0, 0.0);
	}

	void display()
	{
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		applyCamera();

		const GLfloat lightPosition[] = { -12.0f, 28.0f, 16.0f, 1.0f };
		glLightfv(GL_LIGHT0, GL_POSITION, lightPosition);
		drawGround();
		drawPark();
		drawSupports();
		drawTrack();
		drawCart(trackAt(rideProgress));
		drawHud();
		glutSwapBuffers();
	}

	void reshape(int width, int height)
	{
		windowWidth = std::max(1, width);
		windowHeight = std::max(1, height);
		glViewport(0, 0, windowWidth, windowHeight);
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		gluPerspective(55.0, static_cast<double>(windowWidth) / windowHeight, 0.1, 250.0);
		glMatrixMode(GL_MODELVIEW);
	}

	void update(int)
	{
		if (!paused)
		{
			rideProgress += rideSpeed / 60.0f;
			if (rideProgress >= 1.0f)
			{
				rideProgress -= 1.0f;
			}
		}
		glutPostRedisplay();
		glutTimerFunc(16, update, 0);
	}

	void keyboard(unsigned char key, int, int)
	{
		switch (key)
		{
		case 27:
			if (quadric != nullptr)
			{
				gluDeleteQuadric(quadric);
				quadric = nullptr;
			}
			std::exit(0);
			break;
		case ' ':
			paused = !paused;
			break;
		case '+':
		case '=':
			rideSpeed = std::min(0.55f, rideSpeed + 0.03f);
			break;
		case '-':
		case '_':
			rideSpeed = std::max(0.03f, rideSpeed - 0.03f);
			break;
		case 'c':
		case 'C':
			cameraMode = (cameraMode + 1) % 3;
			break;
		case 'l':
		case 'L':
			lightingEnabled = !lightingEnabled;
			if (lightingEnabled) glEnable(GL_LIGHTING); else glDisable(GL_LIGHTING);
			break;
		case 'h':
		case 'H':
			showHud = !showHud;
			break;
		case 'r':
		case 'R':
			rideProgress = 0.0f;
			paused = false;
			rideSpeed = 0.16f;
			break;
		default:
			break;
		}
		glutPostRedisplay();
	}

	void specialKeys(int key, int, int)
	{
		if (key == GLUT_KEY_LEFT) orbitYaw -= 4.0f;
		if (key == GLUT_KEY_RIGHT) orbitYaw += 4.0f;
		if (key == GLUT_KEY_UP) orbitPitch = std::min(78.0f, orbitPitch + 3.0f);
		if (key == GLUT_KEY_DOWN) orbitPitch = std::max(5.0f, orbitPitch - 3.0f);
		glutPostRedisplay();
	}

	void mouseButton(int button, int state, int x, int y)
	{
		if (button == GLUT_LEFT_BUTTON)
		{
			dragging = state == GLUT_DOWN;
			previousMouseX = x;
			previousMouseY = y;
		}
		if (button == 3 && state == GLUT_DOWN) orbitDistance = std::max(15.0f, orbitDistance - 1.5f);
		if (button == 4 && state == GLUT_DOWN) orbitDistance = std::min(70.0f, orbitDistance + 1.5f);
		glutPostRedisplay();
	}

	void mouseMotion(int x, int y)
	{
		if (dragging)
		{
			orbitYaw += (x - previousMouseX) * 0.45f;
			orbitPitch = std::clamp(orbitPitch + (previousMouseY - y) * 0.35f, 5.0f, 78.0f);
			previousMouseX = x;
			previousMouseY = y;
			glutPostRedisplay();
		}
	}

	void initializeOpenGL()
	{
		glClearColor(0.52f, 0.74f, 0.91f, 1.0f);
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LEQUAL);
		glShadeModel(GL_SMOOTH);
		glEnable(GL_COLOR_MATERIAL);
		glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
		glEnable(GL_LIGHTING);
		glEnable(GL_LIGHT0);
		const GLfloat ambient[] = { 0.34f, 0.34f, 0.38f, 1.0f };
		const GLfloat diffuse[] = { 0.95f, 0.91f, 0.82f, 1.0f };
		glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient);
		glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
		glEnable(GL_NORMALIZE);
		quadric = gluNewQuadric();
		if (quadric != nullptr)
		{
			gluQuadricNormals(quadric, GLU_SMOOTH);
		}
	}
}

int main(int argc, char** argv)
{
	buildTrack();
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
	glutInitWindowSize(WindowWidth, WindowHeight);
	glutInitWindowPosition(80, 60);
	glutCreateWindow("ThrillRide 3D | OpenGL Roller Coaster Simulation");
	initializeOpenGL();

	glutDisplayFunc(display);
	glutReshapeFunc(reshape);
	glutKeyboardFunc(keyboard);
	glutSpecialFunc(specialKeys);
	glutMouseFunc(mouseButton);
	glutMotionFunc(mouseMotion);
	glutTimerFunc(16, update, 0);
	glutMainLoop();

	if (quadric != nullptr)
	{
		gluDeleteQuadric(quadric);
	}
	return 0;
}
