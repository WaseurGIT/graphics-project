/*// city_full.cpp
// Semi-realistic city scene using classic OpenGL + freeglut
// Features: buildings with windows/doors, road, cars, sidewalks, walking humans, trees, grass,
// sun + rays, mouse camera rotation & wheel zoom, animation.
// Compile: g++ -o city_full city_full.cpp -lGL -lGLU -lglut

#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glut.h>

#include <vector>
#include <cmath>
#include <cstdlib>

// -------------------------- Global scene & camera params --------------------------
float camAngleY = 0.0f;    // yaw (left-right)
float camAngleX = -18.0f;  // pitch (up-down)
float camDist   = 28.0f;   // distance (zoom)

int lastMouseX = 0, lastMouseY = 0;
bool dragging = false;

int windowWidth = 1000, windowHeight = 700;

// Sun parameters
float sunAngle = 45.0f;     // degrees; controls sun position
const float SUN_RADIUS = 40.0f;

// Timer
const int TIMER_MS = 16;    // ~60 fps

// -------------------------- Scene objects --------------------------
struct Car {
    float laneX;   // x position (lane center)
    float z;       // z position
    float speed;   // speed units per frame
    float r,g,b;   // color
};
std::vector<Car> cars;

struct Human {
    float x, z;       // current position
    float dir;        // direction along sidewalk (+1 or -1)
    float speed;      // movement speed
    float phase;      // for simple arm/leg swing animation
};
std::vector<Human> humans;

// Buildings layout
struct Building {
    float x, z;   // center
    float w, d;   // width (x) and depth (z)
    float h;      // height
};
std::vector<Building> buildings;

// -------------------------- Utility helpers --------------------------
void setMaterialRGB(float r, float g, float b, float shininess = 20.0f) {
    GLfloat amb[] = { r*0.2f, g*0.2f, b*0.2f, 1.0f };
    GLfloat dif[] = { r, g, b, 1.0f };
    GLfloat spec[] = { 0.2f, 0.2f, 0.2f, 1.0f };
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, amb);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, dif);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, spec);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, shininess);
}

// Simple box (centered) helper using glutSolidCube scaled
void drawBox(float cx, float cy, float cz, float sx, float sy, float sz) {
    glPushMatrix();
    glTranslatef(cx, cy, cz);
    glScalef(sx, sy, sz);
    glutSolidCube(1.0f);
    glPopMatrix();
}

// -------------------------- Scene building/initialization --------------------------
void setupBuildings() {
    buildings.clear();
    // create a row of buildings on left and right with gaps for sun rays
    // left side (x negative)
    for (int i = 0; i < 6; ++i) {
        float z = -50.0f + i * 20.0f;
        if (i==2) z += 3.0f; // slight irregularity
        float h = 6.0f + (i % 4) * 2.5f; // variety in heights
        buildings.push_back({ -9.0f, z, 6.0f, 8.0f, h }); // left column
    }
    // right side (x positive)
    for (int i = 0; i < 6; ++i) {
        float z = -50.0f + i * 20.0f;
        float h = 5.0f + (i % 5) * 2.0f;
        buildings.push_back({ 9.0f, z + ((i%2)?-2.0f:2.0f), 6.0f, 8.0f, h });
    }
}

// Initialize cars and humans
void initActors() {
    cars.clear();
    // lanes: two lanes centered at x = -1.2 and x = +1.2
    cars.push_back({ -1.2f, -30.0f, 0.02f, 0.9f, 0.1f, 0.1f }); // red
    cars.push_back({  1.2f, -10.0f, 0.015f, 0.1f, 0.1f, 0.9f }); // blue
    cars.push_back({ -1.2f,  10.0f, 0.018f, 0.1f, 0.8f, 0.2f }); // green
    cars.push_back({  1.2f,  30.0f, 0.016f, 0.95f, 0.6f, 0.12f }); // orange

    humans.clear();
    // create humans walking on sidewalk in front of buildings on both sides
    for (int i = 0; i < 8; ++i) {
        float z = -60.0f + i * 15.0f + (rand()%10 - 5) * 0.4f;
        // left sidewalk (x ~ -6.5)
        humans.push_back({ -6.5f + (rand()%100)/500.0f, z, 1.0f * ((i%2)?1.0f:-1.0f), 0.01f + (rand()%5)/200.0f, (float) (rand()%100)/100.0f });
        // right sidewalk
        humans.push_back({  6.5f + (rand()%100)/500.0f, z + (rand()%10-5), -1.0f * ((i%2)?1.0f:-1.0f), 0.01f + (rand()%5)/200.0f, (float) (rand()%100)/100.0f });
    }
}

// -------------------------- Drawing primitives --------------------------
void drawWindowPanel(int rows, int cols, float b_w, float b_h, float sill_y) {
    // Draw rows x cols windows on a building face of width b_w and height b_h
    float pad = 0.12f;
    float winW = (b_w - (cols+1)*pad) / cols;
    float winH = (b_h - (rows+1)*pad) / rows;

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            float cx = -b_w/2 + pad + (c * (winW + pad)) + winW/2;
            float cy = sill_y + b_h/2 - pad - (r * (winH + pad)) - winH/2;
            // window frame
            glPushMatrix();
              glTranslatef(cx, cy, 0.501f); // slightly in front
              glScalef(winW, winH, 1.0f);
              setMaterialRGB(0.08f, 0.08f, 0.08f, 5.0f); // frame dark
              glutSolidCube(1.0f);
              // glass (inner)
              glTranslatef(0, 0, 0.051f);
              glScalef(0.85f, 0.85f, 1.0f);
              // glass color slightly reflective
              setMaterialRGB(0.6f, 0.75f, 0.9f, 5.0f);
              GLfloat prevEmission[4];
              glGetMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, prevEmission);
              GLfloat emis[4] = {0.02f,0.03f,0.05f,1.0f};
              glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, emis);
              glutSolidCube(1.0f);
              // reset emission
              glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, prevEmission);
            glPopMatrix();
        }
    }
}

// Draw building with windows & door.
// building centered at (bx, bz) with width w, depth d and height h
void drawBuildingWithDetails(const Building &B) {
    float bx = B.x;
    float bz = B.z;
    float w = B.w;
    float d = B.d;
    float h = B.h;

    // main block
    setMaterialRGB(0.58f, 0.58f, 0.62f, 30.0f);
    drawBox(bx, h/2.0f, bz, w, h, d);

    // front face windows & door (assume front faces toward road center -> z direction)
    // We will put windows on the side that faces z=0 (road center).
    float frontZ = bz - d/2.0f; // front face nearer to road (depending on side)
    // But to be consistent, we draw windows on the face towards center (determine by sign)
    bool faceTowardsCenter = (bz > 0.0f) ? (bz - 0.0f > 0) : (bz - 0.0f < 0);
    // We'll just apply windows on the inward-facing side (toward z=0)
    if (bz > 0.0f) frontZ = bz - d/2.0f;
    else frontZ = bz + d/2.0f;

    // Place windows by transforming to that face
    glPushMatrix();
      // move to front face plane
      glTranslatef(bx, h/2.0f, frontZ + ( (bz>0)?0.502f:-0.502f) );
      // rotate if backside
      if (bz < 0.0f) glRotatef(180.0f, 0,1,0);
      float faceW = w * 0.92f;
      float faceH = h * 0.62f;
      // Draw many window panels
      drawWindowPanel( (int) (h/2.2f), 3, faceW, faceH, 0.0f );
      // door at bottom center
      glPushMatrix();
        glTranslatef(0.0f, -h/2.0f + 1.2f, 0.05f);
        glScalef(0.9f, 1.8f, 0.2f);
        setMaterialRGB(0.36f, 0.22f, 0.1f, 10.0f); // door brown
        glutSolidCube(1.0f);
        // door knob
        setMaterialRGB(0.9f, 0.82f, 0.2f, 10.0f);
        glPushMatrix();
          glTranslatef(0.35f, 0.0f, 0.3f);
          glutSolidSphere(0.05f, 8,8);
        glPopMatrix();
      glPopMatrix();
    glPopMatrix();

    // small roof detail
    setMaterialRGB(0.15f, 0.15f, 0.15f, 5.0f);
    drawBox(bx, h + 0.25f, bz, w*1.02f, 0.4f, d*1.02f);
}

// Draw trees as cylinder trunk + cone leaves
void drawTree(float x, float z, float scale=1.0f) {
    // trunk
    setMaterialRGB(0.45f, 0.25f, 0.1f, 10.0f);
    glPushMatrix();
      glTranslatef(x, 0.8f, z);
      glRotatef(-90, 1, 0, 0);
      GLUquadric* q = gluNewQuadric();
      gluCylinder(q, 0.18f*scale, 0.15f*scale, 1.6f*scale, 8, 1);
      gluDeleteQuadric(q);
    glPopMatrix();

    // leaves (three stacked cones)
    setMaterialRGB(0.1f, 0.5f, 0.12f, 10.0f);
    for (int i=0;i<3;i++) {
        glPushMatrix();
          glTranslatef(x, 1.6f + i*0.7f*scale, z);
          glRotatef(-90, 1, 0, 0);
          glutSolidCone(0.9f*scale - 0.2f*i*scale, 1.0f*scale, 12, 4);
        glPopMatrix();
    }
}

// Draw grass patch with some blades
void drawGrassPatch(float x, float z, float w, float d) {
    setMaterialRGB(0.12f, 0.6f, 0.18f, 2.0f);
    glPushMatrix();
      glTranslatef(x, 0.001f, z);
      glBegin(GL_QUADS);
        glNormal3f(0,1,0);
        glVertex3f(-w/2, 0, -d/2);
        glVertex3f( w/2, 0, -d/2);
        glVertex3f( w/2, 0,  d/2);
        glVertex3f(-w/2, 0,  d/2);
      glEnd();
      // some blades (lines)
      glDisable(GL_LIGHTING);
      glColor3f(0.08f, 0.5f, 0.12f);
      glBegin(GL_LINES);
        for (int i=0;i<30;i++) {
          float rx = (rand()%1000)/1000.0f * w - w/2;
          float rz = (rand()%1000)/1000.0f * d - d/2;
          glVertex3f(rx, 0.0f, rz);
          glVertex3f(rx*0.95f, 0.14f + (rand()%20)/200.0f, rz*0.95f);
        }
      glEnd();
      glEnable(GL_LIGHTING);
    glPopMatrix();
}

// Draw a human (simple stick + box) at given x,z with walking phase
void drawHuman(const Human &h) {
    glPushMatrix();
      glTranslatef(h.x, 0.0f, h.z);
      // body (small cube)
      setMaterialRGB(0.8f,0.55f,0.45f, 10.0f);
      glPushMatrix();
        glTranslatef(0.0f, 0.9f, 0.0f);
        glScalef(0.35f, 0.7f, 0.25f);
        glutSolidCube(1.0f);
      glPopMatrix();
      // head
      setMaterialRGB(0.95f, 0.85f, 0.76f, 10.0f);
      glPushMatrix();
        glTranslatef(0.0f, 1.5f, 0.0f);
        glutSolidSphere(0.18f, 10, 8);
      glPopMatrix();
      // legs (two slender boxes) with simple swing using phase
      float swing = sinf(h.phase*6.28f) * 0.25f;
      setMaterialRGB(0.15f, 0.15f, 0.18f, 5.0f);
      glPushMatrix(); // left leg
        glTranslatef(-0.09f + 0.02f*swing, 0.35f, 0.0f);
        glScalef(0.12f, 0.7f, 0.12f);
        glutSolidCube(1.0f);
      glPopMatrix();
      glPushMatrix(); // right leg
        glTranslatef(0.09f - 0.02f*swing, 0.35f, 0.0f);
        glScalef(0.12f, 0.7f, 0.12f);
        glutSolidCube(1.0f);
      glPopMatrix();
      // arms
      setMaterialRGB(0.18f, 0.14f, 0.1f, 5.0f);
      glPushMatrix(); // left arm
        glTranslatef(-0.28f, 1.05f, 0.0f);
        glRotatef(swing*30.0f, 1,0,0);
        glScalef(0.1f, 0.6f, 0.1f);
        glutSolidCube(1.0f);
      glPopMatrix();
      glPushMatrix(); // right arm
        glTranslatef(0.28f, 1.05f, 0.0f);
        glRotatef(-swing*30.0f, 1,0,0);
        glScalef(0.1f, 0.6f, 0.1f);
        glutSolidCube(1.0f);
      glPopMatrix();
    glPopMatrix();
}

// Draw a simple car body with color
void drawCarModel(const Car &c) {
    glPushMatrix();
      glTranslatef(c.laneX, 0.25f, c.z);
      setMaterialRGB(c.r, c.g, c.b, 30.0f);
      glPushMatrix();
        glScalef(0.9f, 0.4f, 1.6f);
        glutSolidCube(1.0f);
      glPopMatrix();
      // cabin
      setMaterialRGB(0.85f, 0.95f, 1.0f, 10.0f);
      glPushMatrix();
        glTranslatef(0.0f, 0.2f, -0.15f);
        glScalef(0.6f, 0.35f, 0.8f);
        glutSolidCube(1.0f);
      glPopMatrix();
      // wheels (simple tori)
      setMaterialRGB(0.02f, 0.02f, 0.02f, 5.0f);
      for (int i=-1;i<=1;i+=2) {
        for (int j=-1;j<=1;j+=2) {
          glPushMatrix();
            glTranslatef(0.35f*j, -0.05f, 0.6f*i);
            glRotatef(90, 0,1,0);
            glutSolidTorus(0.05, 0.09, 8, 10);
          glPopMatrix();
        }
      }
    glPopMatrix();
}

// Draw sun (light & sphere) and soft rays shining down (triangles)
void drawSunAndRays() {
    // compute sun pos by sunAngle (in degrees) along an arc
    float rad = sunAngle * M_PI / 180.0f;
    float sx = SUN_RADIUS * cosf(rad);
    float sy = SUN_RADIUS * sinf(rad) + 6.0f;
    float sz = -10.0f;

    // configure light0 as sun
    GLfloat sunPos[] = { sx, sy, sz, 1.0f };
    GLfloat sunDiff[] = { 1.0f, 0.95f, 0.85f, 1.0f };
    GLfloat sunAmb[]  = { 0.25f, 0.22f, 0.18f, 1.0f };
    glLightfv(GL_LIGHT0, GL_POSITION, sunPos);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, sunDiff);
    glLightfv(GL_LIGHT0, GL_AMBIENT, sunAmb);

    // draw sun sphere (emissive)
    glPushMatrix();
      glTranslatef(sx, sy, sz);
      glDisable(GL_LIGHTING);
      glColor3f(1.0f, 0.95f, 0.6f);
      glutSolidSphere(1.3f, 24, 20);
      glEnable(GL_LIGHTING);
      GLfloat old_em[4];
      glGetMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, old_em);
      GLfloat emis[4] = {0.5f,0.45f,0.25f,1.0f};
      glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, emis);
      glutSolidSphere(0.9f, 20, 16);
      glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, old_em);
    glPopMatrix();

    // soft rays (semi-transparent triangle fans) shining through gaps.
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // create multiple rays pointing down toward road center and gaps
    for (int side = -1; side <= 1; side += 2) { // left and right sides
        float baseX = side * 6.5f;
        for (int g = 0; g < 5; ++g) {
            float gapZ = -40.0f + g * 20.0f + ((rand()%100)/100.0f)*4.0f;
            float width = 2.0f + (rand()%100)/100.0f * 1.6f;
            glColor4f(1.0f, 0.92f, 0.7f, 0.08f);
            glBegin(GL_TRIANGLES);
              glVertex3f(sx, sy, sz);
              glVertex3f(baseX - width, 0.0f, gapZ);
              glVertex3f(baseX + width, 0.0f, gapZ + 6.0f);
            glEnd();
        }
    }

    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
}

// Draw the entire scene
void drawScene() {
    // Ground (large grass area)
    setMaterialRGB(0.16f, 0.55f, 0.2f, 2.0f);
    glBegin(GL_QUADS);
      glNormal3f(0,1,0);
      glVertex3f(-200.0f, 0.0f, -200.0f);
      glVertex3f( 200.0f, 0.0f, -200.0f);
      glVertex3f( 200.0f, 0.0f,  200.0f);
      glVertex3f(-200.0f, 0.0f,  200.0f);
    glEnd();

    // road center (asphalt)
    setMaterialRGB(0.08f, 0.08f, 0.08f, 5.0f);
    glBegin(GL_QUADS);
      glNormal3f(0,1,0);
      glVertex3f(-3.5f, 0.001f, -120.0f);
      glVertex3f( 3.5f, 0.001f, -120.0f);
      glVertex3f( 3.5f, 0.001f,  120.0f);
      glVertex3f(-3.5f, 0.001f,  120.0f);
    glEnd();

    // yellow dashed centerline
    glDisable(GL_LIGHTING);
    glLineWidth(6.0f);
    glBegin(GL_LINES);
      for (float z=-120.0f; z<120.0f; z+=8.0f) {
        glColor3f(1.0f, 0.9f, 0.0f);
        glVertex3f(0.0f, 0.002f, z);
        glVertex3f(0.0f, 0.002f, z+4.0f);
      }
    glEnd();
    glEnable(GL_LIGHTING);

    // sidewalks left and right
    setMaterialRGB(0.5f,0.5f,0.5f, 2.0f);
    glBegin(GL_QUADS);
      glNormal3f(0,1,0);
      glVertex3f(-7.5f, 0.002f, -120.0f);
      glVertex3f(-3.5f, 0.002f, -120.0f);
      glVertex3f(-3.5f, 0.002f, 120.0f);
      glVertex3f(-7.5f, 0.002f, 120.0f);

      glVertex3f(3.5f, 0.002f, -120.0f);
      glVertex3f(7.5f, 0.002f, -120.0f);
      glVertex3f(7.5f, 0.002f, 120.0f);
      glVertex3f(3.5f, 0.002f, 120.0f);
    glEnd();

    // small grass strips between sidewalk and buildings
    drawGrassPatch(-11.0f, 0.0f, 6.0f, 220.0f); // left green strip
    drawGrassPatch(11.0f, 0.0f, 6.0f, 220.0f);  // right green strip

    // draw buildings
    for (const Building &b : buildings) {
        drawBuildingWithDetails(b);
    }

    // trees near sidewalk
    for (int i = 0; i < 7; ++i) {
        float z = -70.0f + i * 24.0f + (i%2?2.0f:-2.0f);
        drawTree(-12.2f, z, 1.0f);
        drawTree(12.2f, z+4.0f, 1.0f);
    }

    // draw cars
    for (const Car &c : cars) drawCarModel(c);

    // humans on sidewalks
    for (const Human &h : humans) drawHuman(h);
}

// -------------------------- OpenGL callbacks --------------------------
void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // camera transform
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    float radY = camAngleY * M_PI / 180.0f;
    float radX = camAngleX * M_PI / 180.0f;

    float eyeX = camDist * cosf(radX) * sinf(radY);
    float eyeY = camDist * sinf(radX);
    float eyeZ = camDist * cosf(radX) * cosf(radY);

    // look at scene center slightly above ground
    gluLookAt(eyeX, eyeY, eyeZ,  0.0f, 2.5f, 0.0f,  0.0f, 1.0f, 0.0f);

    // lights & sun
    drawSunAndRays();

    // draw everything
    drawScene();

    glutSwapBuffers();
}

void reshape(int w, int h) {
    if (h == 0) h = 1;
    windowWidth = w;
    windowHeight = h;
    glViewport(0,0,w,h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(60.0f, (double)w/(double)h, 0.1f, 500.0f);
    glMatrixMode(GL_MODELVIEW);
}

// animation update: move cars, humans, and sun
void update(int value) {
    // move cars slowly along +Z, wrap around
    for (auto &c : cars) {
        c.z += c.speed * 12.0f; // deliberate slow scale
        if (c.z > 120.0f) c.z = -120.0f;
    }

    // humans walk along sidewalks back-and-forth
    for (auto &h : humans) {
        h.z += h.dir * h.speed * 6.0f;
        // simple bouncing when reach ends of sidewalk region
        if (h.z > 110.0f) { h.z = 110.0f; h.dir *= -1.0f; }
        if (h.z < -110.0f) { h.z = -110.0f; h.dir *= -1.0f; }
        h.phase += 0.02f + 0.005f * h.speed; // update animation phase
        if (h.phase > 1000.0f) h.phase -= 1000.0f;
    }

    // gently move sun (slow)
    sunAngle += 0.02f;
    if (sunAngle > 180.0f) sunAngle = 40.0f; // loop back to morning-ish

    glutPostRedisplay();
    glutTimerFunc(TIMER_MS, update, 0);
}

// mouse controls for rotation + wheel for zoom
void mouse(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON) {
        if (state == GLUT_DOWN) {
            dragging = true;
            lastMouseX = x;
            lastMouseY = y;
        } else {
            dragging = false;
        }
    }

    // mouse wheel: some GLUT implementations use button 3/4
    if (button == 3) { // wheel up
        camDist -= 1.0f;
        if (camDist < 5.0f) camDist = 5.0f;
    } else if (button == 4) { // wheel down
        camDist += 1.0f;
        if (camDist > 120.0f) camDist = 120.0f;
    }
    glutPostRedisplay();
}

void motion(int x, int y) {
    if (!dragging) return;
    int dx = x - lastMouseX;
    int dy = y - lastMouseY;
    camAngleY += dx * 0.4f;
    camAngleX += dy * 0.3f;
    if (camAngleX > 80.0f) camAngleX = 80.0f;
    if (camAngleX < -80.0f) camAngleX = -80.0f;
    lastMouseX = x;
    lastMouseY = y;
    glutPostRedisplay();
}

// simple keyboard for additional control
void keyboard(unsigned char key, int x, int y) {
    switch (key) {
        case 27: exit(0); break;
        case 'w': camDist -= 1.0f; if (camDist < 5.0f) camDist = 5.0f; break;
        case 's': camDist += 1.0f; if (camDist > 150.0f) camDist = 150.0f; break;
        case 'a': camAngleY -= 5.0f; break;
        case 'd': camAngleY += 5.0f; break;
        case 'r': camAngleX = -18.0f; camAngleY = 0.0f; camDist=28.0f; break;
    }
    glutPostRedisplay();
}

// -------------------------- Initialization --------------------------
void initGL() {
    glEnable(GL_DEPTH_TEST);
    glShadeModel(GL_SMOOTH);
    glEnable(GL_NORMALIZE);

    // lighting baseline
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    GLfloat global_amb[] = {0.22f, 0.22f, 0.22f, 1.0f};
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, global_amb);

    // nice perspective corrections
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

    // background sky color
    glClearColor(0.53f, 0.81f, 0.98f, 1.0f);

    // material default
    GLfloat defA[] = {0.2f,0.2f,0.2f,1.0f};
    GLfloat defD[] = {0.8f,0.8f,0.8f,1.0f};
    GLfloat defS[] = {0.1f,0.1f,0.1f,1.0f};
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, defA);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, defD);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, defS);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 10.0f);

    // initialize scene objects
    setupBuildings();
    initActors();
}

// -------------------------- Main --------------------------
int main(int argc, char** argv) {
    srand(12345);

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(windowWidth, windowHeight);
    glutCreateWindow("Semi-Realistic City - Buildings, Road, Cars, Trees, People, Sun");

    initGL();

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutMouseFunc(mouse);
    glutMotionFunc(motion);
    glutKeyboardFunc(keyboard);
    glutTimerFunc(TIMER_MS, update, 0);

    glutMainLoop();
    return 0;
}
*/


/* ********************************************* ******************************** ****************************************/


#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glut.h>

#include <vector>
#include <cmath>
#include <cstdlib>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// -------------------------- Global scene & camera params --------------------------
float camAngleY = 0.0f;    // yaw (left-right)
float camAngleX = -18.0f;  // pitch (up-down)
float camDist   = 28.0f;   // distance (zoom)

// Camera Target/Focus Point (this is what moves with arrow keys)
float targetX = 0.0f;
float targetY = 2.5f; // Keep target height constant for horizon view
float targetZ = 0.0f;

const float MOVEMENT_SPEED = 0.8f; // Speed for keyboard translation

int lastMouseX = 0, lastMouseY = 0;
bool dragging = false;

int windowWidth = 1000, windowHeight = 700;

// Sun parameters
float sunAngle = 45.0f;     // degrees; controls sun position
const float SUN_RADIUS = 40.0f;

// Timer
const int TIMER_MS = 16;    // ~60 fps

// -------------------------- Scene objects --------------------------
struct Car {
    float laneX;   // x position (lane center)
    float z;       // z position
    float speed;   // speed units per frame
    float r,g,b;   // color
};
std::vector<Car> cars;

struct Human {
    float x, z;       // current position
    float dir;        // direction along sidewalk (+1 or -1)
    float speed;      // movement speed
    float phase;      // for simple arm/leg swing animation
};
std::vector<Human> humans;

// Buildings layout
struct Building {
    float x, z;   // center
    float w, d;   // width (x) and depth (z)
    float h;      // height
};
std::vector<Building> buildings;

// -------------------------- Utility helpers --------------------------
void setMaterialRGB(float r, float g, float b, float shininess = 20.0f) {
    GLfloat amb[] = { r*0.2f, g*0.2f, b*0.2f, 1.0f };
    GLfloat dif[] = { r, g, b, 1.0f };
    GLfloat spec[] = { 0.2f, 0.2f, 0.2f, 1.0f };
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, amb);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, dif);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, spec);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, shininess);
}

// Simple box (centered) helper using glutSolidCube scaled
void drawBox(float cx, float cy, float cz, float sx, float sy, float sz) {
    glPushMatrix();
    glTranslatef(cx, cy, cz);
    glScalef(sx, sy, sz);
    glutSolidCube(1.0f);
    glPopMatrix();
}

// -------------------------- Scene building/initialization --------------------------
void setupBuildings() {
    buildings.clear();
    // left side (x negative)
    for (int i = 0; i < 6; ++i) {
        float z = -50.0f + i * 20.0f;
        if (i==2) z += 3.0f; // slight irregularity
        float h = 6.0f + (i % 4) * 2.5f; // variety in heights
        buildings.push_back({ -9.0f, z, 6.0f, 8.0f, h }); // left column
    }
    // right side (x positive)
    for (int i = 0; i < 6; ++i) {
        float z = -50.0f + i * 20.0f;
        float h = 5.0f + (i % 5) * 2.0f;
        buildings.push_back({ 9.0f, z + ((i%2)?-2.0f:2.0f), 6.0f, 8.0f, h });
    }
}

// Initialize cars and humans
void initActors() {
    cars.clear();
    // lanes: two lanes centered at x = -1.2 and x = +1.2
    cars.push_back({ -1.2f, -30.0f, 0.02f, 0.9f, 0.1f, 0.1f }); // red
    cars.push_back({  1.2f, -10.0f, 0.015f, 0.1f, 0.1f, 0.9f }); // blue
    cars.push_back({ -1.2f,  10.0f, 0.018f, 0.1f, 0.8f, 0.2f }); // green
    cars.push_back({  1.2f,  30.0f, 0.016f, 0.95f, 0.6f, 0.12f }); // orange

    humans.clear();
    // create humans walking on sidewalk in front of buildings on both sides
    for (int i = 0; i < 8; ++i) {
        float z = -60.0f + i * 15.0f + (rand()%10 - 5) * 0.4f;
        // left sidewalk (x ~ -6.5)
        humans.push_back({ -6.5f + (rand()%100)/500.0f, z, 1.0f * ((i%2)?1.0f:-1.0f), 0.01f + (rand()%5)/200.0f, (float) (rand()%100)/100.0f });
        // right sidewalk
        humans.push_back({  6.5f + (rand()%100)/500.0f, z + (rand()%10-5), -1.0f * ((i%2)?1.0f:-1.0f), 0.01f + (rand()%5)/200.0f, (float) (rand()%100)/100.0f });
    }
}

// -------------------------- Drawing primitives --------------------------
void drawWindowPanel(int rows, int cols, float b_w, float b_h, float sill_y) {
    // Draw rows x cols windows on a building face of width b_w and height b_h
    float pad = 0.12f;
    float winW = (b_w - (cols+1)*pad) / cols;
    float winH = (b_h - (rows+1)*pad) / rows;

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            float cx = -b_w/2 + pad + (c * (winW + pad)) + winW/2;
            float cy = sill_y + b_h/2 - pad - (r * (winH + pad)) - winH/2;
            // window frame
            glPushMatrix();
              glTranslatef(cx, cy, 0.501f); // slightly in front
              glScalef(winW, winH, 1.0f);
              setMaterialRGB(0.08f, 0.08f, 0.08f, 5.0f); // frame dark
              glutSolidCube(1.0f);
              // glass (inner)
              glTranslatef(0, 0, 0.051f);
              glScalef(0.85f, 0.85f, 1.0f);
              // glass color slightly reflective
              setMaterialRGB(0.6f, 0.75f, 0.9f, 5.0f);
              GLfloat prevEmission[4];
              glGetMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, prevEmission);
              GLfloat emis[4] = {0.02f,0.03f,0.05f,1.0f};
              glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, emis);
              glutSolidCube(1.0f);
              // reset emission
              glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, prevEmission);
            glPopMatrix();
        }
    }
}

// Draw building with windows & door.
void drawBuildingWithDetails(const Building &B) {
    float bx = B.x;
    float bz = B.z;
    float w = B.w;
    float d = B.d;
    float h = B.h;

    // main block
    setMaterialRGB(0.58f, 0.58f, 0.62f, 30.0f);
    drawBox(bx, h/2.0f, bz, w, h, d);

    // front face windows & door
    float frontZ = bz - d/2.0f; // front face nearer to road (depending on side)
    if (bx > 0.0f) frontZ = bz + d/2.0f; // Buildings on the right side face -X/center

    // Place windows by transforming to that face
    glPushMatrix();
      // move to front face plane
      glTranslatef(bx, h/2.0f, frontZ + ( (bx>0)?0.502f:-0.502f) );
      // rotate if backside
      if (bx < 0.0f) glRotatef(180.0f, 0,1,0); // Buildings on the left side face +X/center
      float faceW = w * 0.92f;
      float faceH = h * 0.62f;
      // Draw many window panels
      drawWindowPanel( (int) (h/2.2f), 3, faceW, faceH, 0.0f );
      // door at bottom center
      glPushMatrix();
        glTranslatef(0.0f, -h/2.0f + 1.2f, 0.05f);
        glScalef(0.9f, 1.8f, 0.2f);
        setMaterialRGB(0.36f, 0.22f, 0.1f, 10.0f); // door brown
        glutSolidCube(1.0f);
        // door knob
        setMaterialRGB(0.9f, 0.82f, 0.2f, 10.0f);
        glPushMatrix();
          glTranslatef(0.35f, 0.0f, 0.3f);
          glutSolidSphere(0.05f, 8,8);
        glPopMatrix();
      glPopMatrix();
    glPopMatrix();

    // small roof detail
    setMaterialRGB(0.15f, 0.15f, 0.15f, 5.0f);
    drawBox(bx, h + 0.25f, bz, w*1.02f, 0.4f, d*1.02f);
}

// Draw a simple shadow for a building
void drawBuildingShadow(const Building &B, float sunX, float sunY, float sunZ) {
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.0f, 0.0f, 0.0f, 0.3f); // Semi-transparent black/grey shadow

    glPushMatrix();
      // Translate to the building's base
      glTranslatef(B.x, 0.005f, B.z); // Slightly above ground to avoid z-fighting

      // Calculate shadow direction vector from sun to building
      float lightDirX = B.x - sunX;
      float lightDirZ = B.z - sunZ;
      float shadowOffsetX = -lightDirX * 0.05f;
      float shadowOffsetZ = -lightDirZ * 0.05f;

      // Draw a flattened quad representing the shadow base
      glBegin(GL_QUADS);
        glNormal3f(0,1,0);
        // Project corners of the building base onto the ground, offset by sun position
        glVertex3f(-B.w/2 + shadowOffsetX, 0, -B.d/2 + shadowOffsetZ);
        glVertex3f( B.w/2 + shadowOffsetX, 0, -B.d/2 + shadowOffsetZ);
        glVertex3f( B.w/2 + shadowOffsetX, 0,  B.d/2 + shadowOffsetZ);
        glVertex3f(-B.w/2 + shadowOffsetX, 0,  B.d/2 + shadowOffsetZ);
      glEnd();
    glPopMatrix();

    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
}


// Draw trees as cylinder trunk + cone leaves
void drawTree(float x, float z, float scale=1.0f) {
    // trunk
    setMaterialRGB(0.45f, 0.25f, 0.1f, 10.0f);
    glPushMatrix();
      glTranslatef(x, 0.8f, z);
      glRotatef(-90, 1, 0, 0);
      GLUquadric* q = gluNewQuadric();
      gluCylinder(q, 0.18f*scale, 0.15f*scale, 1.6f*scale, 8, 1);
      gluDeleteQuadric(q);
    glPopMatrix();

    // leaves (three stacked cones)
    setMaterialRGB(0.1f, 0.5f, 0.12f, 10.0f);
    for (int i=0;i<3;i++) {
        glPushMatrix();
          glTranslatef(x, 1.6f + i*0.7f*scale, z);
          glRotatef(-90, 1, 0, 0);
          glutSolidCone(0.9f*scale - 0.2f*i*scale, 1.0f*scale, 12, 4);
        glPopMatrix();
    }
}

// Draw grass patch with some blades
void drawGrassPatch(float x, float z, float w, float d) {
    setMaterialRGB(0.12f, 0.6f, 0.18f, 2.0f);
    glPushMatrix();
      glTranslatef(x, 0.001f, z);
      glBegin(GL_QUADS);
        glNormal3f(0,1,0);
        glVertex3f(-w/2, 0, -d/2);
        glVertex3f( w/2, 0, -d/2);
        glVertex3f( w/2, 0,  d/2);
        glVertex3f(-w/2, 0,  d/2);
      glEnd();
      // some blades (lines)
      glDisable(GL_LIGHTING);
      glColor3f(0.08f, 0.5f, 0.12f);
      glBegin(GL_LINES);
        for (int i=0;i<30;i++) {
          float rx = (rand()%1000)/1000.0f * w - w/2;
          float rz = (rand()%1000)/1000.0f * d - d/2;
          glVertex3f(rx, 0.0f, rz);
          glVertex3f(rx*0.95f, 0.14f + (rand()%20)/200.0f, rz*0.95f);
        }
      glEnd();
      glEnable(GL_LIGHTING);
    glPopMatrix();
}

// Draw a human (simple stick + box) at given x,z with walking phase
void drawHuman(const Human &h) {
    glPushMatrix();
      glTranslatef(h.x, 0.0f, h.z);
      // body (small cube)
      setMaterialRGB(0.8f,0.55f,0.45f, 10.0f);
      glPushMatrix();
        glTranslatef(0.0f, 0.9f, 0.0f);
        glScalef(0.35f, 0.7f, 0.25f);
        glutSolidCube(1.0f);
      glPopMatrix();
      // head
      setMaterialRGB(0.95f, 0.85f, 0.76f, 10.0f);
      glPushMatrix();
        glTranslatef(0.0f, 1.5f, 0.0f);
        glutSolidSphere(0.18f, 10, 8);
      glPopMatrix();
      // legs (two slender boxes) with simple swing using phase
      float swing = sinf(h.phase*6.28f) * 0.25f;
      setMaterialRGB(0.15f, 0.15f, 0.18f, 5.0f);
      glPushMatrix(); // left leg
        glTranslatef(-0.09f + 0.02f*swing, 0.35f, 0.0f);
        glScalef(0.12f, 0.7f, 0.12f);
        glutSolidCube(1.0f);
      glPopMatrix();
      glPushMatrix(); // right leg
        glTranslatef(0.09f - 0.02f*swing, 0.35f, 0.0f);
        glScalef(0.12f, 0.7f, 0.12f);
        glutSolidCube(1.0f);
      glPopMatrix();
      // arms
      setMaterialRGB(0.18f, 0.14f, 0.1f, 5.0f);
      glPushMatrix(); // left arm
        glTranslatef(-0.28f, 1.05f, 0.0f);
        glRotatef(swing*30.0f, 1,0,0);
        glScalef(0.1f, 0.6f, 0.1f);
        glutSolidCube(1.0f);
      glPopMatrix();
      glPushMatrix(); // right arm
        glTranslatef(0.28f, 1.05f, 0.0f);
        glRotatef(-swing*30.0f, 1,0,0);
        glScalef(0.1f, 0.6f, 0.1f);
        glutSolidCube(1.0f);
      glPopMatrix();
    glPopMatrix();
}

// Draw a simple car body with color
void drawCarModel(const Car &c) {
    glPushMatrix();
      glTranslatef(c.laneX, 0.25f, c.z);
      setMaterialRGB(c.r, c.g, c.b, 30.0f);
      glPushMatrix();
        glScalef(0.9f, 0.4f, 1.6f);
        glutSolidCube(1.0f);
      glPopMatrix();
      // cabin
      setMaterialRGB(0.85f, 0.95f, 1.0f, 10.0f);
      glPushMatrix();
        glTranslatef(0.0f, 0.2f, -0.15f);
        glScalef(0.6f, 0.35f, 0.8f);
        glutSolidCube(1.0f);
      glPopMatrix();
      // wheels (simple tori)
      setMaterialRGB(0.02f, 0.02f, 0.02f, 5.0f);
      for (int i=-1;i<=1;i+=2) {
        for (int j=-1;j<=1;j+=2) {
          glPushMatrix();
            glTranslatef(0.35f*j, -0.05f, 0.6f*i);
            glRotatef(90, 0,1,0);
            glutSolidTorus(0.05, 0.09, 8, 10);
          glPopMatrix();
        }
      }
    glPopMatrix();
}

// Draw sun (light & sphere) and get its position
void drawSunAndRays(float& sunX_out, float& sunY_out, float& sunZ_out) {
    // compute sun pos by sunAngle (in degrees) along an arc
    float rad = sunAngle * M_PI / 180.0f;
    float sx = SUN_RADIUS * cosf(rad);
    float sy = SUN_RADIUS * sinf(rad) + 6.0f;
    float sz = -10.0f;

    // Output sun position for shadow calculations
    sunX_out = sx;
    sunY_out = sy;
    sunZ_out = sz;

    // configure light0 as sun (Light Golden Color)
    GLfloat sunPos[] = { sx, sy, sz, 1.0f };
    // Light golden color
    GLfloat sunDiff[] = { 1.0f, 0.88f, 0.55f, 1.0f };
    GLfloat sunAmb[]  = { 0.28f, 0.23f, 0.15f, 1.0f };
    glLightfv(GL_LIGHT0, GL_POSITION, sunPos);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, sunDiff);
    glLightfv(GL_LIGHT0, GL_AMBIENT, sunAmb);

    // draw sun sphere (emissive)
    glPushMatrix();
      glTranslatef(sx, sy, sz);
      glDisable(GL_LIGHTING);
      glColor3f(1.0f, 0.9f, 0.5f); // Golden color for solid sphere
      glutSolidSphere(1.3f, 24, 20);
      glEnable(GL_LIGHTING);
      GLfloat old_em[4];
      glGetMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, old_em);
      GLfloat emis[4] = {0.6f,0.5f,0.3f,1.0f}; // Brighter golden emission
      glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, emis);
      glutSolidSphere(0.9f, 20, 16);
      glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, old_em);
    glPopMatrix();
}

// Draw the entire scene
void drawScene(float currentSunX, float currentSunY, float currentSunZ) {
    // Ground (large grass area)
    setMaterialRGB(0.16f, 0.55f, 0.2f, 2.0f);
    glBegin(GL_QUADS);
      glNormal3f(0,1,0);
      glVertex3f(-200.0f, 0.0f, -200.0f);
      glVertex3f( 200.0f, 0.0f, -200.0f);
      glVertex3f( 200.0f, 0.0f,  200.0f);
      glVertex3f(-200.0f, 0.0f,  200.0f);
    glEnd();

    // road center (asphalt)
    setMaterialRGB(0.08f, 0.08f, 0.08f, 5.0f);
    glBegin(GL_QUADS);
      glNormal3f(0,1,0);
      glVertex3f(-3.5f, 0.001f, -120.0f);
      glVertex3f( 3.5f, 0.001f, -120.0f);
      glVertex3f( 3.5f, 0.001f,  120.0f);
      glVertex3f(-3.5f, 0.001f,  120.0f);
    glEnd();

    // yellow dashed centerline
    glDisable(GL_LIGHTING);
    glLineWidth(6.0f);
    glBegin(GL_LINES);
      for (float z=-120.0f; z<120.0f; z+=8.0f) {
        glColor3f(1.0f, 0.9f, 0.0f);
        glVertex3f(0.0f, 0.002f, z);
        glVertex3f(0.0f, 0.002f, z+4.0f);
      }
    glEnd();
    glEnable(GL_LIGHTING);

    // sidewalks left and right
    setMaterialRGB(0.5f,0.5f,0.5f, 2.0f);
    glBegin(GL_QUADS);
      glNormal3f(0,1,0);
      glVertex3f(-7.5f, 0.002f, -120.0f);
      glVertex3f(-3.5f, 0.002f, -120.0f);
      glVertex3f(-3.5f, 0.002f, 120.0f);
      glVertex3f(-7.5f, 0.002f, 120.0f);

      glVertex3f(3.5f, 0.002f, -120.0f);
      glVertex3f(7.5f, 0.002f, -120.0f);
      glVertex3f(7.5f, 0.002f, 120.0f);
      glVertex3f(3.5f, 0.002f, 120.0f);
    glEnd();

    // small grass strips between sidewalk and buildings
    drawGrassPatch(-11.0f, 0.0f, 6.0f, 220.0f); // left green strip
    drawGrassPatch(11.0f, 0.0f, 6.0f, 220.0f);  // right green strip

    // Draw building shadows first
    for (const Building &b : buildings) {
        drawBuildingShadow(b, currentSunX, currentSunY, currentSunZ);
    }

    // then draw buildings
    for (const Building &b : buildings) {
        drawBuildingWithDetails(b);
    }

    // trees near sidewalk
    for (int i = 0; i < 7; ++i) {
        float z = -70.0f + i * 24.0f + (i%2?2.0f:-2.0f);
        drawTree(-12.2f, z, 1.0f);
        drawTree(12.2f, z+4.0f, 1.0f);
    }

    // draw cars
    for (const Car &c : cars) drawCarModel(c);

    // humans on sidewalks
    for (const Human &h : humans) drawHuman(h);
}

// -------------------------- OpenGL callbacks --------------------------
void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // camera transform
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // 1. Calculate the rotation angles in radians
    float radY = camAngleY * M_PI / 180.0f;
    float radX = camAngleX * M_PI / 180.0f;

    // 2. Calculate the camera eye position relative to the target (orbit)
    float relX = camDist * cosf(radX) * sinf(radY);
    float relY = camDist * sinf(radX);
    float relZ = camDist * cosf(radX) * cosf(radY);

    // 3. Calculate the absolute eye position
    float eyeX = targetX + relX;
    float eyeY = targetY + relY;
    float eyeZ = targetZ + relZ;

    // 4. Look at the target point
    gluLookAt(eyeX, eyeY, eyeZ,  targetX, targetY, targetZ,  0.0f, 1.0f, 0.0f);

    // lights & sun
    float sunX, sunY, sunZ; // These will get the sun's position
    drawSunAndRays(sunX, sunY, sunZ);

    // draw everything
    drawScene(sunX, sunY, sunZ); // Pass sun position to drawScene for shadows

    glutSwapBuffers();
}

void reshape(int w, int h) {
    if (h == 0) h = 1;
    windowWidth = w;
    windowHeight = h;
    glViewport(0,0,w,h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(60.0f, (double)w/(double)h, 0.1f, 500.0f);
    glMatrixMode(GL_MODELVIEW);
}

// animation update: move cars, humans, and sun
void update(int value) {
    // move cars slowly along +Z, wrap around
    for (auto &c : cars) {
        c.z += c.speed * 12.0f; // deliberate slow scale
        if (c.z > 120.0f) c.z = -120.0f;
    }

    // humans walk along sidewalks back-and-forth
    for (auto &h : humans) {
        h.z += h.dir * h.speed * 6.0f;
        // simple bouncing when reach ends of sidewalk region
        if (h.z > 110.0f) { h.z = 110.0f; h.dir *= -1.0f; }
        if (h.z < -110.0f) { h.z = -110.0f; h.dir *= -1.0f; }
        h.phase += 0.02f + 0.005f * h.speed; // update animation phase
        if (h.phase > 1000.0f) h.phase -= 1000.0f;
    }

    // gently move sun (slow)
    sunAngle += 0.02f;
    if (sunAngle > 180.0f) sunAngle = 40.0f; // loop back to morning-ish

    glutPostRedisplay();
    glutTimerFunc(TIMER_MS, update, 0);
}

// mouse controls for rotation + wheel for zoom
void mouse(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON) {
        if (state == GLUT_DOWN) {
            dragging = true;
            lastMouseX = x;
            lastMouseY = y;
        } else {
            dragging = false;
        }
    }

    // mouse wheel: zoom in/out
    if (button == 3) { // wheel up (zoom in)
        camDist -= 1.0f;
        if (camDist < 5.0f) camDist = 5.0f;
    } else if (button == 4) { // wheel down (zoom out)
        camDist += 1.0f;
        if (camDist > 120.0f) camDist = 120.0f;
    }
    glutPostRedisplay();
}

void motion(int x, int y) {
    if (!dragging) return;
    int dx = x - lastMouseX;
    int dy = y - lastMouseY;
    camAngleY += dx * 0.4f;
    camAngleX += dy * 0.3f;
    if (camAngleX > 80.0f) camAngleX = 80.0f;
    if (camAngleX < -80.0f) camAngleX = -80.0f;
    lastMouseX = x;
    lastMouseY = y;
    glutPostRedisplay();
}

// Special keyboard function for directional movement (Arrow Keys)
void specialKeyboard(int key, int x, int y) {
    float radY = camAngleY * M_PI / 180.0f;

    // Calculate forward/backward direction vector components (X and Z)
    // The negative sign is because the camera orbits a target. To move the
    // target in the direction the camera is looking, we need to move it in
    // the opposite direction of the orbit vector components.
    float forwardX = -sinf(radY) * MOVEMENT_SPEED;
    float forwardZ = -cosf(radY) * MOVEMENT_SPEED;

    // Strafe (perpendicular) direction vector components (X and Z)
    float strafeX = cosf(radY) * MOVEMENT_SPEED;
    float strafeZ = -sinf(radY) * MOVEMENT_SPEED;

    switch (key) {
        case GLUT_KEY_UP:
            // Forward movement: move the target along the view vector
            targetX += forwardX;
            targetZ += forwardZ;
            break;
        case GLUT_KEY_DOWN:
            // Backward movement
            targetX -= forwardX;
            targetZ -= forwardZ;
            break;
        case GLUT_KEY_LEFT:
            // Strafe Left
            targetX -= strafeX;
            targetZ -= strafeZ;
            break;
        case GLUT_KEY_RIGHT:
            // Strafe Right
            targetX += strafeX;
            targetZ += strafeZ;
            break;
    }
    glutPostRedisplay();
}

// simple keyboard for additional control (ASCII keys)
void keyboard(unsigned char key, int x, int y) {
    switch (key) {
        case 27: exit(0); break;
        case 'w': camDist -= 1.0f; if (camDist < 5.0f) camDist = 5.0f; break; // Zoom In
        case 's': camDist += 1.0f; if (camDist > 150.0f) camDist = 150.0f; break; // Zoom Out
        case 'a': camAngleY -= 5.0f; break; // Rotate Left
        case 'd': camAngleY += 5.0f; break; // Rotate Right
        case 'r': // Reset view
            camAngleX = -18.0f; camAngleY = 0.0f; camDist=28.0f;
            targetX = 0.0f; targetY = 2.5f; targetZ = 0.0f;
            break;
    }
    glutPostRedisplay();
}

// -------------------------- Initialization --------------------------
void initGL() {
    glEnable(GL_DEPTH_TEST);
    glShadeModel(GL_SMOOTH);
    glEnable(GL_NORMALIZE);

    // lighting baseline
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    GLfloat global_amb[] = {0.22f, 0.22f, 0.22f, 1.0f};
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, global_amb);

    // nice perspective corrections
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

    // background sky color
    glClearColor(0.53f, 0.81f, 0.98f, 1.0f);

    // material default setup...
    GLfloat defA[] = {0.2f,0.2f,0.2f,1.0f};
    GLfloat defD[] = {0.8f,0.8f,0.8f,1.0f};
    GLfloat defS[] = {0.1f,0.1f,0.1f,1.0f};
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, defA);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, defD);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, defS);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 10.0f);

    // initialize scene objects
    setupBuildings();
    initActors();
}

// -------------------------- Main --------------------------
int main(int argc, char** argv) {
    srand(12345);

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(windowWidth, windowHeight);
    glutCreateWindow("Semi-Realistic City - Directional Camera Control Added (Arrow Keys)");

    initGL();

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutMouseFunc(mouse);
    glutMotionFunc(motion);
    glutKeyboardFunc(keyboard);
    // Register the special keyboard function for non-ASCII keys (like arrow keys)
    glutSpecialFunc(specialKeyboard);
    glutTimerFunc(TIMER_MS, update, 0);

    glutMainLoop();
    return 0;
}

