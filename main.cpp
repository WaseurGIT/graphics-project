#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glut.h>

#include <vector>
#include <cmath>
#include <cstdlib>
#include <ctime>

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

// Weather system
enum WeatherType { SUNNY, RAINY };
WeatherType currentWeather = SUNNY;
float weatherTimer = 0.0f;
const float WEATHER_CHANGE_TIME = 10.0f; // Change weather every 10 seconds
float rainIntensity = 0.0f;
std::vector<std::pair<float, float>> rainDrops; // x, z positions

// -------------------------- Scene objects --------------------------
struct Car {
    float laneX;   // x position (lane center)
    float z;       // z position
    float speed;   // speed units per frame
    float r,g,b;   // color
    int carType;   // 0: sedan, 1: SUV, 2: sports car, 3: truck
    float wheelRotation; // for rotating wheels
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
void setMaterialRGB(float r, float g, float b, float shininess)
{
    GLfloat amb[]  = { r * 0.2f, g * 0.2f, b * 0.2f, 1.0f };
    GLfloat dif[]  = { r, g, b, 1.0f };
    GLfloat spec[] = { 0.8f, 0.8f, 0.8f, 1.0f };
    GLfloat shine  = shininess;

    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT,  amb);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE,  dif);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, spec);
    glMaterialf (GL_FRONT_AND_BACK, GL_SHININESS, shine);
}

// Simple box (centered) helper using glutSolidCube scaled
void drawBox(float cx, float cy, float cz, float sx, float sy, float sz) {
    glPushMatrix();
    glTranslatef(cx, cy, cz);
    glScalef(sx, sy, sz);
    glutSolidCube(1.0f);
    glPopMatrix();
}

// -------------------------- Weather system --------------------------
void initRain() {
    rainDrops.clear();
    for (int i = 0; i < 500; i++) { // Create 500 rain drops
        rainDrops.push_back(std::make_pair(
            (rand() % 200) - 100.0f,  // x: -100 to 100
            (rand() % 200) - 100.0f   // z: -100 to 100
        ));
    }
}

void updateWeather(float deltaTime) {
    weatherTimer += deltaTime;

    if (weatherTimer >= WEATHER_CHANGE_TIME) {
        weatherTimer = 0.0f;

        if (currentWeather == SUNNY) {
            currentWeather = RAINY;
            rainIntensity = 0.0f;
            initRain();
        } else {
            currentWeather = SUNNY;
            rainIntensity = 0.0f;
        }
    }

    // Smooth transition between weather states
    if (currentWeather == RAINY) {
        rainIntensity = fmin(rainIntensity + deltaTime * 0.5f, 1.0f);
    } else {
        rainIntensity = fmax(rainIntensity - deltaTime * 0.5f, 0.0f);
    }
}

void updateRain(float deltaTime) {
    for (auto& drop : rainDrops) {
        // Move drops downward
        drop.second -= deltaTime * 50.0f * rainIntensity; // Move faster with higher intensity

        // Reset drops that fall below ground level
        if (drop.second < -100.0f) {
            drop.first = (rand() % 200) - 100.0f;
            drop.second = 100.0f + (rand() % 50);
        }
    }
}

void drawRain() {
    if (rainIntensity <= 0.0f) return;

    glDisable(GL_LIGHTING);
    glColor4f(0.7f, 0.7f, 1.0f, 0.6f * rainIntensity);
    glLineWidth(1.0f);

    glBegin(GL_LINES);
    for (const auto& drop : rainDrops) {
        float y = 20.0f + fmod(drop.second * 0.3f, 5.0f); // Vary height slightly
        glVertex3f(drop.first, y, drop.second);
        glVertex3f(drop.first, y - 2.0f, drop.second - 0.5f); // Angled rain
    }
    glEnd();

    glEnable(GL_LIGHTING);
}

void setWeatherLighting(float& sunX, float& sunY, float& sunZ) {
    if (currentWeather == SUNNY) {
        // Sunny weather - bright and warm
        GLfloat sunDiff[] = { 1.0f, 0.88f, 0.55f, 1.0f };
        GLfloat sunAmb[]  = { 0.28f, 0.23f, 0.15f, 1.0f };
        GLfloat global_amb[] = {0.22f, 0.22f, 0.22f, 1.0f};

        glLightfv(GL_LIGHT0, GL_DIFFUSE, sunDiff);
        glLightfv(GL_LIGHT0, GL_AMBIENT, sunAmb);
        glLightModelfv(GL_LIGHT_MODEL_AMBIENT, global_amb);

        // Set sky color to bright blue
        glClearColor(0.53f, 0.81f, 0.98f, 1.0f);
    } else {
        // Rainy weather - dark and cool
        GLfloat rainDiff[] = { 0.4f, 0.4f, 0.5f, 1.0f };
        GLfloat rainAmb[]  = { 0.15f, 0.15f, 0.2f, 1.0f };
        GLfloat global_amb[] = {0.1f, 0.1f, 0.15f, 1.0f};

        glLightfv(GL_LIGHT0, GL_DIFFUSE, rainDiff);
        glLightfv(GL_LIGHT0, GL_AMBIENT, rainAmb);
        glLightModelfv(GL_LIGHT_MODEL_AMBIENT, global_amb);

        // Set sky color to gray
        glClearColor(0.4f, 0.4f, 0.5f, 1.0f);
    }
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

void initActors() {
    cars.clear();
    // Two lanes:
    // - Left lane (x = -1.2f): cars moving in +Z direction (forward)
    // - Right lane (x = 1.2f): cars moving in -Z direction (backward)

    // Left lane cars (moving forward +Z)
    cars.push_back({ -1.2f, -30.0f, 0.02f, 0.9f, 0.1f, 0.1f, 0, 0.0f }); // red sedan
    cars.push_back({ -1.2f, -10.0f, 0.018f, 0.1f, 0.8f, 0.2f, 2, 0.0f }); // green sports car

    // Right lane cars (moving backward -Z) - negative speed
    cars.push_back({  1.2f, 30.0f, -0.015f, 0.1f, 0.1f, 0.9f, 1, 0.0f }); // blue SUV
    cars.push_back({  1.2f, 10.0f, -0.016f, 0.95f, 0.6f, 0.12f, 3, 0.0f }); // orange truck

    humans.clear();
    for (int i = 0; i < 8; ++i) {
        float z = -60.0f + i * 15.0f + (rand()%10 - 5) * 0.4f;

        // Left sidewalk humans
        humans.push_back({
            -4.8f + (rand()%100)/500.0f,
            z,
            1.0f * ((i%2)?1.0f:-1.0f),
            0.005f + (rand()%3)/300.0f,
            (float) (rand()%100)/100.0f
        });

        // Right sidewalk humans
        humans.push_back({
            4.8f + (rand()%100)/500.0f,
            z + (rand()%10-5),
            -1.0f * ((i%2)?1.0f:-1.0f),
            0.005f + (rand()%3)/300.0f,
            (float) (rand()%100)/100.0f
        });
    }
}

// -------------------------- Drawing primitives --------------------------
void drawWindowPanel(int rows, int cols, float b_w, float b_h, float sill_y) {
    float pad = 0.15f;
    float winW = (b_w - (cols+1)*pad) / cols;
    float winH = (b_h - (rows+1)*pad) / rows;

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            float cx = -b_w/2 + pad + (c * (winW + pad)) + winW/2;
            float cy = sill_y + b_h/2 - pad - (r * (winW + pad)) - winH/2;

            glPushMatrix();
              glTranslatef(cx, cy, 0.0f);

              // Window frame
              glPushMatrix();
                glTranslatef(0, 0, -0.1f);
                glScalef(winW, winH, 0.15f);
                setMaterialRGB(0.15f, 0.15f, 0.15f, 5.0f);
                glutSolidCube(1.0f);
              glPopMatrix();

              // Glass pane
              glPushMatrix();
                glTranslatef(0, 0, 0.02f);
                glScalef(winW * 0.85f, winH * 0.85f, 0.01f);

                // Adjust glass color based on weather
                if (currentWeather == SUNNY) {
                    setMaterialRGB(0.7f, 0.85f, 1.0f, 80.0f);
                } else {
                    setMaterialRGB(0.5f, 0.6f, 0.8f, 60.0f); // Darker glass for rainy weather
                }

                GLfloat prevEmission[4];
                glGetMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, prevEmission);
                GLfloat emis[4] = {0.1f, 0.12f, 0.15f, 1.0f};
                glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, emis);
                glutSolidCube(1.0f);
                glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, prevEmission);
              glPopMatrix();

              // Window sill
              glPushMatrix();
                glTranslatef(0, -winH/2 - 0.02f, 0.05f);
                glScalef(winW * 1.1f, 0.04f, 0.1f);
                setMaterialRGB(0.3f, 0.3f, 0.3f, 10.0f);
                glutSolidCube(1.0f);
              glPopMatrix();

            glPopMatrix();
        }
    }
}

void drawBuildingWithDetails(const Building &B) {
    float bx = B.x;
    float bz = B.z;
    float w = B.w;
    float d = B.d;
    float h = B.h;

    // Adjust building color based on weather
    if (currentWeather == SUNNY) {
        setMaterialRGB(0.58f, 0.58f, 0.62f, 30.0f);
    } else {
        setMaterialRGB(0.45f, 0.45f, 0.5f, 20.0f); // Darker, wet look
    }
    drawBox(bx, h/2.0f, bz, w, h, d);

    // Draw windows on all four sides
    float faceW, faceH;

    // Front face
    glPushMatrix();
      glTranslatef(bx, h/2.0f, bz - d/2.0f);
      glRotatef(180.0f, 0,1,0);
      faceW = w * 0.92f;
      faceH = h * 0.62f;
      drawWindowPanel( (int) (h/2.2f), 3, faceW, faceH, 0.0f );

      // door
      glPushMatrix();
        glTranslatef(0.0f, -h/2.0f + 1.2f, 0.1f);
        glScalef(0.9f, 1.8f, 0.15f);
        setMaterialRGB(0.36f, 0.22f, 0.1f, 10.0f);
        glutSolidCube(1.0f);
        // door knob
        setMaterialRGB(0.9f, 0.82f, 0.2f, 10.0f);
        glPushMatrix();
          glTranslatef(0.35f, 0.0f, 0.5f);
          glutSolidSphere(0.05f, 8,8);
        glPopMatrix();
      glPopMatrix();
    glPopMatrix();

    // Back face
    glPushMatrix();
      glTranslatef(bx, h/2.0f, bz + d/2.0f);
      faceW = w * 0.92f;
      faceH = h * 0.62f;
      drawWindowPanel( (int) (h/2.2f), 3, faceW, faceH, 0.0f );
    glPopMatrix();

    // Left side face
    glPushMatrix();
      glTranslatef(bx - w/2.0f, h/2.0f, bz);
      glRotatef(-90.0f, 0,1,0);
      faceW = d * 0.92f;
      faceH = h * 0.62f;
      drawWindowPanel( (int) (h/2.2f), 2, faceW, faceH, 0.0f );
    glPopMatrix();

    // Right side face
    glPushMatrix();
      glTranslatef(bx + w/2.0f, h/2.0f, bz);
      glRotatef(90.0f, 0,1,0);
      faceW = d * 0.92f;
      faceH = h * 0.62f;
      drawWindowPanel( (int) (h/2.2f), 2, faceW, faceH, 0.0f );
    glPopMatrix();

    // Roof detail - darker when rainy
    if (currentWeather == SUNNY) {
        setMaterialRGB(0.15f, 0.15f, 0.15f, 5.0f);
    } else {
        setMaterialRGB(0.1f, 0.1f, 0.12f, 3.0f); // Darker, wet roof
    }
    drawBox(bx, h + 0.25f, bz, w*1.02f, 0.4f, d*1.02f);
}

void drawBuildingShadow(const Building &B, float sunX, float sunY, float sunZ) {
    // Don't draw shadows during heavy rain
    if (currentWeather == RAINY && rainIntensity > 0.7f) return;

    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Lighter shadows during rainy weather
    if (currentWeather == SUNNY) {
        glColor4f(0.0f, 0.0f, 0.0f, 0.3f);
    } else {
        glColor4f(0.0f, 0.0f, 0.0f, 0.15f * (1.0f - rainIntensity));
    }

    glPushMatrix();
      glTranslatef(B.x, 0.005f, B.z);
      float lightDirX = B.x - sunX;
      float lightDirZ = B.z - sunZ;
      float shadowOffsetX = -lightDirX * 0.05f;
      float shadowOffsetZ = -lightDirZ * 0.05f;

      glBegin(GL_QUADS);
        glNormal3f(0,1,0);
        glVertex3f(-B.w/2 + shadowOffsetX, 0, -B.d/2 + shadowOffsetZ);
        glVertex3f( B.w/2 + shadowOffsetX, 0, -B.d/2 + shadowOffsetZ);
        glVertex3f( B.w/2 + shadowOffsetX, 0,  B.d/2 + shadowOffsetZ);
        glVertex3f(-B.w/2 + shadowOffsetX, 0,  B.d/2 + shadowOffsetZ);
      glEnd();
    glPopMatrix();

    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
}

void drawTree(float x, float z, float scale=1.0f) {
    // trunk
    if (currentWeather == SUNNY) {
        setMaterialRGB(0.45f, 0.25f, 0.1f, 10.0f);
    } else {
        setMaterialRGB(0.35f, 0.2f, 0.08f, 8.0f); // Darker, wet trunk
    }
    glPushMatrix();
      glTranslatef(x, 0.8f, z);
      glRotatef(-90, 1, 0, 0);
      GLUquadric* q = gluNewQuadric();
      gluCylinder(q, 0.18f*scale, 0.15f*scale, 1.6f*scale, 8, 1);
      gluDeleteQuadric(q);
    glPopMatrix();

    // leaves
    if (currentWeather == SUNNY) {
        setMaterialRGB(0.1f, 0.5f, 0.12f, 10.0f);
    } else {
        setMaterialRGB(0.08f, 0.4f, 0.1f, 8.0f); // Darker, wet leaves
    }
    for (int i=0;i<3;i++) {
        glPushMatrix();
          glTranslatef(x, 1.6f + i*0.7f*scale, z);
          glRotatef(-90, 1, 0, 0);
          glutSolidCone(0.9f*scale - 0.2f*i*scale, 1.0f*scale, 12, 4);
        glPopMatrix();
    }
}

void drawGrassPatch(float x, float z, float w, float d) {
    // Base grass surface - adjust color based on weather
    if (currentWeather == SUNNY) {
        setMaterialRGB(0.16f, 0.55f, 0.2f, 2.0f);
    } else {
        setMaterialRGB(0.12f, 0.45f, 0.16f, 1.0f); // Darker, wet grass
    }

    glPushMatrix();
      glTranslatef(x, 0.001f, z);
      glBegin(GL_QUADS);
        glNormal3f(0,1,0);
        glVertex3f(-w/2, 0, -d/2);
        glVertex3f( w/2, 0, -d/2);
        glVertex3f( w/2, 0,  d/2);
        glVertex3f(-w/2, 0,  d/2);
      glEnd();
    glPopMatrix();

    // Detailed grass blades
    glDisable(GL_LIGHTING);
    glPushMatrix();
      glTranslatef(x, 0.0f, z);

      GLfloat darkGreen[3], mediumGreen[3], lightGreen[3];
      if (currentWeather == SUNNY) {
          darkGreen[0]=0.08f; darkGreen[1]=0.45f; darkGreen[2]=0.12f;
          mediumGreen[0]=0.12f; mediumGreen[1]=0.55f; mediumGreen[2]=0.15f;
          lightGreen[0]=0.15f; lightGreen[1]=0.65f; lightGreen[2]=0.18f;
      } else {
          darkGreen[0]=0.06f; darkGreen[1]=0.35f; darkGreen[2]=0.1f;
          mediumGreen[0]=0.09f; mediumGreen[1]=0.45f; mediumGreen[2]=0.12f;
          lightGreen[0]=0.12f; lightGreen[1]=0.55f; lightGreen[2]=0.14f;
      }

      glLineWidth(1.5f);
      glBegin(GL_LINES);
        for (int i=0; i < 200; i++) {
          float rx = (rand()%1000)/1000.0f * w - w/2;
          float rz = (rand()%1000)/1000.0f * d - d/2;
          float height = 0.15f + (rand()%30)/200.0f;
          float curve = (rand()%100)/500.0f - 0.1f;
          float leanX = (rand()%100)/300.0f - 0.16f;
          float leanZ = (rand()%100)/300.0f - 0.16f;

          int shade = rand() % 3;
          switch(shade) {
            case 0: glColor3fv(darkGreen); break;
            case 1: glColor3fv(mediumGreen); break;
            case 2: glColor3fv(lightGreen); break;
          }

          glVertex3f(rx, 0.0f, rz);
          glVertex3f(rx + leanX + curve, height, rz + leanZ);
        }
      glEnd();

    glPopMatrix();
    glEnable(GL_LIGHTING);
}

void drawHuman(const Human &h) {
    glPushMatrix();
      glTranslatef(h.x, 0.0f, h.z);

      // body
      if (currentWeather == SUNNY) {
          setMaterialRGB(0.8f,0.55f,0.45f, 10.0f);
      } else {
          setMaterialRGB(0.7f,0.5f,0.4f, 8.0f); // Darker skin tone in rain
      }

      glPushMatrix();
        glTranslatef(0.0f, 0.9f, 0.0f);
        glScalef(0.35f, 0.7f, 0.25f);
        glutSolidCube(1.0f);
      glPopMatrix();

      // head
      if (currentWeather == SUNNY) {
          setMaterialRGB(0.95f, 0.85f, 0.76f, 10.0f);
      } else {
          setMaterialRGB(0.85f, 0.75f, 0.66f, 8.0f);
      }

      glPushMatrix();
        glTranslatef(0.0f, 1.5f, 0.0f);
        glutSolidSphere(0.18f, 10, 8);
      glPopMatrix();

      // legs
      setMaterialRGB(0.15f, 0.15f, 0.18f, 5.0f);
      float swing = sinf(h.phase*6.28f) * 0.25f;
      glPushMatrix();
        glTranslatef(-0.09f + 0.02f*swing, 0.35f, 0.0f);
        glScalef(0.12f, 0.7f, 0.12f);
        glutSolidCube(1.0f);
      glPopMatrix();
      glPushMatrix();
        glTranslatef(0.09f - 0.02f*swing, 0.35f, 0.0f);
        glScalef(0.12f, 0.7f, 0.12f);
        glutSolidCube(1.0f);
      glPopMatrix();

      // arms
      setMaterialRGB(0.18f, 0.14f, 0.1f, 5.0f);
      glPushMatrix();
        glTranslatef(-0.28f, 1.05f, 0.0f);
        glRotatef(swing*30.0f, 1,0,0);
        glScalef(0.1f, 0.6f, 0.1f);
        glutSolidCube(1.0f);
      glPopMatrix();
      glPushMatrix();
        glTranslatef(0.28f, 1.05f, 0.0f);
        glRotatef(-swing*30.0f, 1,0,0);
        glScalef(0.1f, 0.6f, 0.1f);
        glutSolidCube(1.0f);
      glPopMatrix();
    glPopMatrix();
}

// Car drawing functions (sedan, SUV, sports car, truck) remain the same
void drawSedan(const Car &c) {
    glPushMatrix();
      glTranslatef(c.laneX, 0.3f, c.z);
      setMaterialRGB(c.r, c.g, c.b, 30.0f);
      glPushMatrix();
        glScalef(1.0f, 0.45f, 2.2f);
        glutSolidCube(1.0f);
      glPopMatrix();
      setMaterialRGB(0.85f, 0.95f, 1.0f, 10.0f);
      glPushMatrix();
        glTranslatef(0.0f, 0.25f, -0.3f);
        glScalef(0.7f, 0.4f, 1.0f);
        glutSolidCube(1.0f);
      glPopMatrix();
      setMaterialRGB(0.9f, 0.9f, 0.7f, 50.0f);
      glPushMatrix();
        glTranslatef(0.4f, 0.1f, 0.9f);
        glutSolidSphere(0.08f, 8, 8);
      glPopMatrix();
      glPushMatrix();
        glTranslatef(-0.4f, 0.1f, 0.9f);
        glutSolidSphere(0.08f, 8, 8);
      glPopMatrix();
      setMaterialRGB(0.02f, 0.02f, 0.02f, 5.0f);
      for (int i=-1;i<=1;i+=2) {
        for (int j=-1;j<=1;j+=2) {
          glPushMatrix();
            glTranslatef(0.55f*j, -0.15f, 0.6f*i);
            glRotatef(90, 0,1,0);
            glRotatef(c.wheelRotation, 0,0,1);
            glutSolidTorus(0.08, 0.12, 8, 12);
          glPopMatrix();
        }
      }
    glPopMatrix();
}

void drawSUV(const Car &c) {
    glPushMatrix();
      glTranslatef(c.laneX, 0.4f, c.z);
      setMaterialRGB(c.r, c.g, c.b, 30.0f);
      glPushMatrix();
        glScalef(1.2f, 0.6f, 2.4f);
        glutSolidCube(1.0f);
      glPopMatrix();
      setMaterialRGB(0.85f, 0.95f, 1.0f, 10.0f);
      glPushMatrix();
        glTranslatef(0.0f, 0.35f, -0.2f);
        glScalef(0.9f, 0.5f, 1.2f);
        glutSolidCube(1.0f);
      glPopMatrix();
      setMaterialRGB(0.3f, 0.3f, 0.3f, 10.0f);
      glPushMatrix();
        glTranslatef(0.0f, 0.7f, 0.0f);
        glScalef(0.8f, 0.05f, 1.8f);
        glutSolidCube(1.0f);
      glPopMatrix();
      setMaterialRGB(0.02f, 0.02f, 0.02f, 5.0f);
      for (int i=-1;i<=1;i+=2) {
        for (int j=-1;j<=1;j+=2) {
          glPushMatrix();
            glTranslatef(0.65f*j, -0.2f, 0.7f*i);
            glRotatef(90, 0,1,0);
            glRotatef(c.wheelRotation, 0,0,1);
            glutSolidTorus(0.1, 0.15, 8, 12);
          glPopMatrix();
        }
      }
    glPopMatrix();
}

void drawSportsCar(const Car &c) {
    glPushMatrix();
      glTranslatef(c.laneX, 0.25f, c.z);
      setMaterialRGB(c.r, c.g, c.b, 60.0f);
      glPushMatrix();
        glScalef(0.9f, 0.3f, 1.8f);
        glutSolidCube(1.0f);
      glPopMatrix();
      setMaterialRGB(0.2f, 0.2f, 0.2f, 40.0f);
      glPushMatrix();
        glTranslatef(0.0f, 0.2f, -0.2f);
        glScalef(0.7f, 0.25f, 0.9f);
        glutSolidCube(1.0f);
      glPopMatrix();
      setMaterialRGB(c.r*0.7f, c.g*0.7f, c.b*0.7f, 30.0f);
      glPushMatrix();
        glTranslatef(0.0f, 0.4f, -0.8f);
        glScalef(0.6f, 0.05f, 0.2f);
        glutSolidCube(1.0f);
      glPopMatrix();
      setMaterialRGB(0.02f, 0.02f, 0.02f, 5.0f);
      for (int i=-1;i<=1;i+=2) {
        for (int j=-1;j<=1;j+=2) {
          glPushMatrix();
            glTranslatef(0.5f*j, -0.1f, 0.5f*i);
            glRotatef(90, 0,1,0);
            glRotatef(c.wheelRotation, 0,0,1);
            glutSolidTorus(0.06, 0.1, 8, 12);
          glPopMatrix();
        }
      }
    glPopMatrix();
}

void drawTruck(const Car &c) {
    glPushMatrix();
      glTranslatef(c.laneX, 0.5f, c.z);
      setMaterialRGB(c.r, c.g, c.b, 30.0f);
      glPushMatrix();
        glTranslatef(0.0f, 0.3f, -0.8f);
        glScalef(1.0f, 0.8f, 1.0f);
        glutSolidCube(1.0f);
      glPopMatrix();
      setMaterialRGB(c.r*0.8f, c.g*0.8f, c.b*0.8f, 30.0f);
      glPushMatrix();
        glTranslatef(0.0f, 0.4f, 0.8f);
        glScalef(1.4f, 0.9f, 2.0f);
        glutSolidCube(1.0f);
      glPopMatrix();
      setMaterialRGB(0.02f, 0.02f, 0.02f, 5.0f);
      float wheelPositions[] = {-0.7f, 0.7f, -1.5f, 1.5f};
      for (int i=0; i<4; i++) {
        for (int j=-1;j<=1;j+=2) {
          glPushMatrix();
            glTranslatef(wheelPositions[i]*j, -0.3f, -0.5f);
            glRotatef(90, 0,1,0);
            glRotatef(c.wheelRotation, 0,0,1);
            glutSolidTorus(0.12, 0.18, 8, 12);
          glPopMatrix();
        }
      }
      for (int j=-1;j<=1;j+=2) {
        glPushMatrix();
          glTranslatef(wheelPositions[3]*j, -0.3f, 1.2f);
          glRotatef(90, 0,1,0);
          glRotatef(c.wheelRotation, 0,0,1);
          glutSolidTorus(0.12, 0.18, 8, 12);
        glPopMatrix();
      }
    glPopMatrix();
}

void drawCarModel(const Car &c) {
    switch(c.carType) {
        case 0: drawSedan(c); break;
        case 1: drawSUV(c); break;
        case 2: drawSportsCar(c); break;
        case 3: drawTruck(c); break;
        default: drawSedan(c); break;
    }
}

void drawSunAndRays(float& sunX_out, float& sunY_out, float& sunZ_out) {
    float rad = sunAngle * M_PI / 180.0f;
    float sx = SUN_RADIUS * cosf(rad);
    float sy = SUN_RADIUS * sinf(rad) + 6.0f;
    float sz = -10.0f;

    sunX_out = sx;
    sunY_out = sy;
    sunZ_out = sz;

    GLfloat sunPos[] = { sx, sy, sz, 1.0f };
    glLightfv(GL_LIGHT0, GL_POSITION, sunPos);

    // Set weather-based lighting
    setWeatherLighting(sx, sy, sz);

    // Draw sun (only visible during sunny weather)
    if (currentWeather == SUNNY || rainIntensity < 0.5f) {
        glPushMatrix();
          glTranslatef(sx, sy, sz);
          glDisable(GL_LIGHTING);
          glColor3f(1.0f, 0.9f, 0.5f);
          glutSolidSphere(1.3f, 24, 20);
          glEnable(GL_LIGHTING);
          GLfloat old_em[4];
          glGetMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, old_em);
          GLfloat emis[4] = {0.6f,0.5f,0.3f,1.0f};
          glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, emis);
          glutSolidSphere(0.9f, 20, 16);
          glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, old_em);
        glPopMatrix();
    }
}

void drawScene(float currentSunX, float currentSunY, float currentSunZ) {
    // Ground
    if (currentWeather == SUNNY) {
        setMaterialRGB(0.16f, 0.55f, 0.2f, 2.0f);
    } else {
        setMaterialRGB(0.12f, 0.45f, 0.16f, 1.0f);
    }

    glBegin(GL_QUADS);
      glNormal3f(0,1,0);
      glVertex3f(-200.0f, 0.0f, -200.0f);
      glVertex3f( 200.0f, 0.0f, -200.0f);
      glVertex3f( 200.0f, 0.0f,  200.0f);
      glVertex3f(-200.0f, 0.0f,  200.0f);
    glEnd();

    // Road - darker when wet
    if (currentWeather == SUNNY) {
        setMaterialRGB(0.08f, 0.08f, 0.08f, 5.0f);
    } else {
        setMaterialRGB(0.05f, 0.05f, 0.06f, 3.0f);
    }

    glBegin(GL_QUADS);
      glNormal3f(0,1,0);
      glVertex3f(-3.5f, 0.001f, -120.0f);
      glVertex3f( 3.5f, 0.001f, -120.0f);
      glVertex3f( 3.5f, 0.001f,  120.0f);
      glVertex3f(-3.5f, 0.001f,  120.0f);
    glEnd();

    // Road markings
    glDisable(GL_LIGHTING);
    glLineWidth(3.0f);
    glColor3f(1.0f, 0.9f, 0.0f);
    glBegin(GL_LINES);
      for (float z=-120.0f; z<120.0f; z+=8.0f) {
        glVertex3f(0.0f, 0.002f, z);
        glVertex3f(0.0f, 0.002f, z+4.0f);
      }
    glEnd();
    glColor3f(1.0f, 1.0f, 1.0f);
    glBegin(GL_LINES);
      for (float z=-120.0f; z<120.0f; z+=15.0f) {
        glVertex3f(-0.6f, 0.002f, z);
        glVertex3f(-0.6f, 0.002f, z+7.0f);
      }
      for (float z=-120.0f; z<120.0f; z+=15.0f) {
        glVertex3f(0.6f, 0.002f, z);
        glVertex3f(0.6f, 0.002f, z+7.0f);
      }
    glEnd();
    glEnable(GL_LIGHTING);

    // Sidewalks
    if (currentWeather == SUNNY) {
        setMaterialRGB(0.5f,0.5f,0.5f, 2.0f);
    } else {
        setMaterialRGB(0.4f,0.4f,0.45f, 1.0f);
    }

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

    // Grass strips
    drawGrassPatch(-11.0f, 0.0f, 6.0f, 220.0f);
    drawGrassPatch(11.0f, 0.0f, 6.0f, 220.0f);

    // Draw building shadows
    for (const Building &b : buildings) {
        drawBuildingShadow(b, currentSunX, currentSunY, currentSunZ);
    }

    // Draw buildings
    for (const Building &b : buildings) {
        drawBuildingWithDetails(b);
    }

    // Draw trees
    float leftTreePositions[] = {-16.5f, -15.0f, -17.0f, -14.5f, -16.0f, -15.5f, -17.5f, -14.0f, -16.8f, -15.2f};
    float leftTreeZPositions[] = {-85.0f, -65.0f, -45.0f, -25.0f, -5.0f, 15.0f, 35.0f, 55.0f, 75.0f, 95.0f};
    for (int i = 0; i < 10; ++i) {
        drawTree(leftTreePositions[i], leftTreeZPositions[i], 0.9f + (i % 3) * 0.1f);
    }
    float rightTreePositions[] = {16.5f, 15.0f, 17.0f, 14.5f, 16.0f, 15.5f, 17.5f, 14.0f, 16.8f, 15.2f};
    float rightTreeZPositions[] = {-80.0f, -60.0f, -40.0f, -20.0f, 0.0f, 20.0f, 40.0f, 60.0f, 80.0f, 100.0f};
    for (int i = 0; i < 10; ++i) {
        drawTree(rightTreePositions[i], rightTreeZPositions[i], 0.95f + (i % 3) * 0.1f);
    }
    for (int i = 0; i < 6; ++i) {
        drawTree(-28.0f + (i % 3) * 2.0f, -90.0f + i * 35.0f, 1.2f);
        drawTree(28.0f - (i % 3) * 2.0f, -85.0f + i * 33.0f, 1.2f);
    }

    // Draw cars
    for (const Car &c : cars) drawCarModel(c);

    // Draw humans
    for (const Human &h : humans) drawHuman(h);

    // Draw rain
    drawRain();
}

// -------------------------- OpenGL callbacks --------------------------
void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    float radY = camAngleY * M_PI / 180.0f;
    float radX = camAngleX * M_PI / 180.0f;
    float relX = camDist * cosf(radX) * sinf(radY);
    float relY = camDist * sinf(radX);
    float relZ = camDist * cosf(radX) * cosf(radY);
    float eyeX = targetX + relX;
    float eyeY = targetY + relY;
    float eyeZ = targetZ + relZ;
    gluLookAt(eyeX, eyeY, eyeZ,  targetX, targetY, targetZ,  0.0f, 1.0f, 0.0f);

    float sunX, sunY, sunZ;
    drawSunAndRays(sunX, sunY, sunZ);
    drawScene(sunX, sunY, sunZ);

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

void update(int value) {
    float deltaTime = TIMER_MS / 1000.0f;

    // Update weather system
    updateWeather(deltaTime);
    if (currentWeather == RAINY) {
        updateRain(deltaTime);
    }

    // Move cars
    for (auto &c : cars) {
        c.z += c.speed * 12.0f;
        c.wheelRotation += c.speed * 300.0f;
        if (c.speed > 0) {
            if (c.z > 120.0f) c.z = -120.0f;
        } else {
            if (c.z < -120.0f) c.z = 120.0f;
        }
        if (c.wheelRotation > 360.0f) c.wheelRotation -= 360.0f;
        if (c.wheelRotation < -360.0f) c.wheelRotation += 360.0f;
    }

    // Move humans
    for (auto &h : humans) {
        h.z += h.dir * h.speed * 6.0f;
        if (h.z > 110.0f) { h.z = 110.0f; h.dir *= -1.0f; }
        if (h.z < -110.0f) { h.z = -110.0f; h.dir *= -1.0f; }
        h.phase += 0.02f + 0.005f * h.speed;
        if (h.phase > 1000.0f) h.phase -= 1000.0f;
    }

    // Move sun
    sunAngle += 0.02f;
    if (sunAngle > 180.0f) sunAngle = 40.0f;

    glutPostRedisplay();
    glutTimerFunc(TIMER_MS, update, 0);
}

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
    if (button == 3) {
        camDist -= 1.0f;
        if (camDist < 5.0f) camDist = 5.0f;
    } else if (button == 4) {
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

void specialKeyboard(int key, int x, int y) {
    float radY = camAngleY * M_PI / 180.0f;
    float forwardX = -sinf(radY) * MOVEMENT_SPEED;
    float forwardZ = -cosf(radY) * MOVEMENT_SPEED;
    float strafeX = cosf(radY) * MOVEMENT_SPEED;
    float strafeZ = -sinf(radY) * MOVEMENT_SPEED;

    switch (key) {
        case GLUT_KEY_UP:
            targetX += forwardX;
            targetZ += forwardZ;
            break;
        case GLUT_KEY_DOWN:
            targetX -= forwardX;
            targetZ -= forwardZ;
            break;
        case GLUT_KEY_LEFT:
            targetX -= strafeX;
            targetZ -= strafeZ;
            break;
        case GLUT_KEY_RIGHT:
            targetX += strafeX;
            targetZ += strafeZ;
            break;
    }
    glutPostRedisplay();
}

void keyboard(unsigned char key, int x, int y) {
    switch (key) {
        case 27: exit(0); break;
        case 'w': camDist -= 1.0f; if (camDist < 5.0f) camDist = 5.0f; break;
        case 's': camDist += 1.0f; if (camDist > 150.0f) camDist = 150.0f; break;
        case 'a': camAngleY -= 5.0f; break;
        case 'd': camAngleY += 5.0f; break;
        case 'r':
            camAngleX = -18.0f; camAngleY = 0.0f; camDist=28.0f;
            targetX = 0.0f; targetY = 2.5f; targetZ = 0.0f;
            break;
        case ' ': // Space bar to manually toggle weather
            if (currentWeather == SUNNY) {
                currentWeather = RAINY;
                rainIntensity = 0.0f;
                initRain();
            } else {
                currentWeather = SUNNY;
                rainIntensity = 0.0f;
            }
            weatherTimer = 0.0f;
            break;
    }
    glutPostRedisplay();
}

void initGL() {
    glEnable(GL_DEPTH_TEST);
    glShadeModel(GL_SMOOTH);
    glEnable(GL_NORMALIZE);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);

    GLfloat global_amb[] = {0.22f, 0.22f, 0.22f, 1.0f};
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, global_amb);
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

    glClearColor(0.53f, 0.81f, 0.98f, 1.0f);

    GLfloat defA[] = {0.2f,0.2f,0.2f,1.0f};
    GLfloat defD[] = {0.8f,0.8f,0.8f,1.0f};
    GLfloat defS[] = {0.1f,0.1f,0.1f,1.0f};
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, defA);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, defD);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, defS);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 10.0f);

    setupBuildings();
    initActors();
    initRain(); // Initialize rain system
}

int main(int argc, char** argv) {
    srand(time(0)); // Use time-based seed for better randomness

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(windowWidth, windowHeight);
    glutCreateWindow("Semi-Realistic City with Dynamic Weather");

    initGL();

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutMouseFunc(mouse);
    glutMotionFunc(motion);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(specialKeyboard);
    glutTimerFunc(TIMER_MS, update, 0);

    glutMainLoop();
    return 0;
}
