/* San Angeles Observation OpenGL ES version example
 * Copyright 2004-2005 Jetro Lauha
 * All rights reserved.
 * Web: http://iki.fi/jetro/
 *
 * This source is free software; you can redistribute it and/or
 * modify it under the terms of EITHER:
 *   (1) The GNU Lesser General Public License as published by the Free
 *       Software Foundation; either version 2.1 of the License, or (at
 *       your option) any later version. The text of the GNU Lesser
 *       General Public License is included with this source in the
 *       file LICENSE-LGPL.txt.
 *   (2) The BSD-style license that is included with this source in
 *       the file LICENSE-BSD.txt.
 *
 * This source is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the files
 * LICENSE-LGPL.txt and LICENSE-BSD.txt for more details.
 *
 * $Id: demo.c,v 1.10 2005/02/08 20:54:39 tonic Exp $
 * $Revision: 1.10 $
 */
#include <stdlib.h>
#include <math.h>
#include <assert.h>

#include "importgl.h"

#include "app.h"
#include "shapes.h"
#include "cams.h"

#include <android/log.h>

// Total run length is 20 * camera track base unit length (see cams.h).
#define RUN_LENGTH  (20 * CAMTRACK_LEN)
#undef PI
#define PI 3.1415926535897932f
#define NUMBOXES 1
#define ONE_OVER_RADICAL_2 0.070710678118f
//#define RANDOM_UINT_MAX 65535


static unsigned long sRandomSeed = 0;

static void seedRandom(unsigned long seed) {
    sRandomSeed = seed;
}

static unsigned long randomUInt() {
    sRandomSeed = sRandomSeed * 0x343fd + 0x269ec3;
    return sRandomSeed >> 16;
}


// Capped conversion from float to fixed.
static long floatToFixed(float value) {
    if (value < -32768) value = -32768;
    if (value > 32767) value = 32767;
    return (long) (value * 65536);
}

#define FIXED(value) floatToFixed(value)


// Definition of one GL object in this demo.
typedef struct {
    /* Vertex array and color array are enabled for all objects, so their
     * pointers must always be valid and non-NULL. Normal array is not
     * used by the ground plane, so when its pointer is NULL then normal
     * array usage is disabled.
     *
     * Vertex array is supposed to use GL_FIXED datatype and stride 0
     * (i.e. tightly packed array). Color array is supposed to have 4
     * components per color with GL_UNSIGNED_BYTE datatype and stride 0.
     * Normal array is supposed to use GL_FIXED datatype and stride 0.
     */
    GLfixed *vertexArray;
    GLubyte *colorArray;
    GLfixed *normalArray;
    GLint vertexComponents;
    GLsizei count;
} GLOBJECT;


static long sStartTick = 0;
static long sTick = 0;

static GLOBJECT *sSuperShapeObjects[SUPERSHAPE_COUNT] = {NULL};
static GLOBJECT *sGroundPlane = NULL;
//static GLOBJECT *sBoxes[NUMBOXES];


typedef struct {
    float x, y, z;
} VECTOR3;


static void freeGLObject(GLOBJECT *object) {
    if (object == NULL)
        return;
    free(object->normalArray);
    free(object->colorArray);
    free(object->vertexArray);
    free(object);
}


static GLOBJECT *newGLObject(long vertices, int vertexComponents,
                             int useNormalArray) {
    GLOBJECT *result;
    result = (GLOBJECT *) malloc(sizeof(GLOBJECT));
    if (result == NULL)
        return NULL;
    result->count = (GLsizei) vertices;
    result->vertexComponents = vertexComponents;
    result->vertexArray = (GLfixed *) malloc(vertices * vertexComponents *
                                             sizeof(GLfixed));
    result->colorArray = (GLubyte *) malloc(vertices * 4 * sizeof(GLubyte));
    if (useNormalArray) {
        result->normalArray = (GLfixed *) malloc(vertices * 3 *
                                                 sizeof(GLfixed));
    }
    else
        result->normalArray = NULL;
    if (result->vertexArray == NULL ||
        result->colorArray == NULL ||
        (useNormalArray && result->normalArray == NULL)) {
        freeGLObject(result);
        return NULL;
    }
    return result;
}


static void drawGLObject(GLOBJECT *object) {
    if (object == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "vf", "drawGLObject object was null");
        return;
    }
    assert(object != NULL);

    glVertexPointer(object->vertexComponents, GL_FIXED,
                    0, object->vertexArray);
    glColorPointer(4, GL_UNSIGNED_BYTE, 0, object->colorArray);

//     Already done in initialization:
//    glEnableClientState(GL_VERTEX_ARRAY);
//    glEnableClientState(GL_COLOR_ARRAY);

    if (object->normalArray) {
        glNormalPointer(GL_FIXED, 0, object->normalArray);
        glEnableClientState(GL_NORMAL_ARRAY);
    }
    else
        glDisableClientState(GL_NORMAL_ARRAY);
    glDrawArrays(GL_TRIANGLES, 0, object->count);
}


//static void vector3Sub(VECTOR3 *dest, VECTOR3 *v1, VECTOR3 *v2) {
//    dest->x = v1->x - v2->x;
//    dest->y = v1->y - v2->y;
//    dest->z = v1->z - v2->z;
//}


//static void assignVertex(int pi, int m, GLOBJECT *result, long p[][3]) {
//    int i;
//    GLubyte color;
//    GLubyte c;
//    for (i = 0; i < 3; i++) {
//        __android_log_print(ANDROID_LOG_DEBUG, "vf", "vertex[%d] = %d", m * 3 + i,
//                            (int) p[pi - 1][i]);
//        result->vertexArray[m * 3 + i] = (GLfixed) p[pi - 1][i];
//
//        int j;
//        color = (GLubyte) ((randomUInt() & 0x5f) + 81);
//        for (j = 0; j < 4; j++) {
//            c = (GLubyte) (j == 3 ? 0 : color);
//            result->colorArray[(m * 3 + i) * 4 + j] = c;
//            __android_log_print(ANDROID_LOG_DEBUG, "vf",
//                                "color[%d] = %d", (m * 3 + i) * 4 + j, c);
//        }
//    }
//}
//
//static void assignNormal(int pi, GLOBJECT *result) {
//    int i;
////    __android_log_print(ANDROID_LOG_DEBUG, "vf", "assign normal to triangle %d", pi/3+1);
//    VECTOR3 r1, r2, r3, v1, v2, n;
//
//    r1.x = result->vertexArray[pi * 3];
//    r1.y = result->vertexArray[pi * 3 + 1];
//    r1.z = result->vertexArray[pi * 3 + 2];
//
//    r2.x = result->vertexArray[pi * 3 + 3];
//    r2.y = result->vertexArray[pi * 3 + 4];
//    r2.z = result->vertexArray[pi * 3 + 5];
//
//    r3.x = result->vertexArray[pi * 3 + 6];
//    r3.y = result->vertexArray[pi * 3 + 7];
//    r3.z = result->vertexArray[pi * 3 + 8];
//
////    __android_log_print(ANDROID_LOG_DEBUG, "vf", "r1 = (%2.2f, %2.2f, %2.2f)",
////                        r1.x, r1.y, r1.z);
////    __android_log_print(ANDROID_LOG_DEBUG, "vf", "r2 = (%2.2f, %2.2f, %2.2f)",
////                        r2.x, r2.y, r2.z);
////    __android_log_print(ANDROID_LOG_DEBUG, "vf", "r3 = (%2.2f, %2.2f, %2.2f)",
////                        r3.x, r3.y, r3.z);
//
//    vector3Sub(&v1, &r2, &r1);
//    vector3Sub(&v2, &r3, &r2);
//
////    __android_log_print(ANDROID_LOG_DEBUG, "vf", "v1 = (%2.2f, %2.2f, %2.2f)",
////                        v1.x, v1.y, v1.z);
////    __android_log_print(ANDROID_LOG_DEBUG, "vf", "v2 = (%2.2f, %2.2f, %2.2f)",
////                        v2.x, v2.y, v2.z);
////
////    __android_log_print(ANDROID_LOG_DEBUG, "vf", "n.x = %2.2f * %2.2f - %2.2f * %2.2f",
////                        v1.y, v2.z, v1.z, v2.y);
////    __android_log_print(ANDROID_LOG_DEBUG, "vf", "n.y = %2.2f * %2.2f - %2.2f * %2.2f",
////                        v1.z, v2.x, v1.x, v2.z);
////    __android_log_print(ANDROID_LOG_DEBUG, "vf", "n.z = %2.2f * %2.2f - %2.2f * %2.2f",
////                        v1.x, v2.y, v1.y, v2.x);
//
//    n.x = v1.y * v2.z - v1.z * v2.y;
//    n.y = v1.z * v2.x - v1.x * v2.z;
//    n.z = v1.x * v2.y - v1.y * v2.x;
//
//    float mag = (float) sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
//    if (mag) {
//        n.x /= mag;
//        n.y /= mag;
//        n.z /= mag;
//    }
//
//    float nvector[3] = {n.x, n.y, n.z};
//
//    for (i = 0; i < 3; i++) {
//        result->normalArray[pi * 3 + i] = (GLfixed) nvector[i];
//    }
//    __android_log_print(ANDROID_LOG_DEBUG, "vf", "normal for triangle %d = (%2.2f, %2.2f, %2.2f)",
//                        pi / 3 + 1, nvector[0], nvector[1], nvector[2]);
//}

//static GLOBJECT *createBox(long x, long y, long z, long w, long l, long h) {
//    const long triangleCount = 12;
//    const long vertices = triangleCount * 3;
//    GLOBJECT *result;
//
//    long p[8][3] = {{x,     y,     z},
//                    {x + w, y,     z},
//                    {x + w, y + l, z},
//                    {x,     y + l, z},
//                    {x,     y,     z + h},
//                    {x + w, y,     z + h},
//                    {x + w, y + l, z + h},
//                    {x,     y + l, z + h}};
//
//    result = newGLObject(vertices, 3, 1);
//
//    assignVertex(1, 0, result, p);
//    assignVertex(2, 1, result, p);
//    assignVertex(6, 2, result, p);
//    assignNormal(0, result);
//
//    assignVertex(6, 3, result, p);
//    assignVertex(5, 4, result, p);
//    assignVertex(1, 5, result, p);
//    assignNormal(3, result);
//
//    assignVertex(2, 6, result, p);
//    assignVertex(3, 7, result, p);
//    assignVertex(7, 8, result, p);
//    assignNormal(6, result);
//
//    assignVertex(7, 9, result, p);
//    assignVertex(6, 10, result, p);
//    assignVertex(2, 11, result, p);
//    assignNormal(9, result);
//
//    assignVertex(3, 12, result, p);
//    assignVertex(4, 13, result, p);
//    assignVertex(8, 14, result, p);
//    assignNormal(12, result);
//
//    assignVertex(8, 15, result, p);
//    assignVertex(7, 16, result, p);
//    assignVertex(3, 17, result, p);
//    assignNormal(15, result);
//
//    assignVertex(4, 18, result, p);
//    assignVertex(1, 19, result, p);
//    assignVertex(5, 20, result, p);
//    assignNormal(18, result);
//
//    assignVertex(5, 21, result, p);
//    assignVertex(8, 22, result, p);
//    assignVertex(4, 23, result, p);
//    assignNormal(21, result);
//
//    assignVertex(5, 24, result, p);
//    assignVertex(6, 25, result, p);
//    assignVertex(7, 26, result, p);
//    assignNormal(24, result);
//
//    assignVertex(7, 27, result, p);
//    assignVertex(8, 28, result, p);
//    assignVertex(5, 29, result, p);
//    assignNormal(27, result);
//
//    assignVertex(2, 30, result, p);
//    assignVertex(1, 31, result, p);
//    assignVertex(4, 32, result, p);
//    assignNormal(30, result);
//
//    assignVertex(4, 33, result, p);
//    assignVertex(3, 34, result, p);
//    assignVertex(2, 35, result, p);
//    assignNormal(33, result);
//
//    return result;
//
//}

static GLOBJECT *createGroundPlane() {
    const int scale = 1;
    const int yBegin = -2, yEnd = 2;    // ends are non-inclusive
    const int xBegin = -2, xEnd = 2;
    const long triangleCount = (yEnd - yBegin) * (xEnd - xBegin) * 2;
    const long vertices = triangleCount * 3;
    GLOBJECT *result;
    int x, y;
    long currentVertex, currentQuad;

    result = newGLObject(vertices, 2, 0);
    if (result == NULL)
        return NULL;

    currentQuad = 0;
    currentVertex = 0;

    for (y = yBegin; y < yEnd; ++y) {
        for (x = xBegin; x < xEnd; ++x) {
            GLubyte color;
            int i, a;
            color = (GLubyte) ((randomUInt() & 0x5f) + 81);  // 101 1111
            for (i = (int) currentVertex * 4; i < (currentVertex + 6) * 4; i += 4) {
                result->colorArray[i] = color;
                result->colorArray[i + 1] = color;
                result->colorArray[i + 2] = color;
                result->colorArray[i + 3] = 0;
            }

            // Axis bits for quad triangles:
            // x: 011100 (0x1c), y: 110001 (0x31)  (clockwise)
            // x: 001110 (0x0e), y: 100011 (0x23)  (counter-clockwise)
            for (a = 0; a < 6; ++a) {
                const int xm = x + ((0x1c >> a) & 1);
                const int ym = y + ((0x31 >> a) & 1);
                const float m = (float) (cos(xm * 2) * sin(ym * 4) * 0.75f);
                result->vertexArray[(int) currentVertex * 2] =
                        (int) FIXED(xm * scale + m);
                result->vertexArray[(int) currentVertex * 2 + 1] =
                        (int) FIXED(ym * scale + m);
                ++currentVertex;
            }
            ++currentQuad;
        }
    }
    return result;
}


static void drawGroundPlane() {
    return;
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ZERO, GL_SRC_COLOR);
    glDisable(GL_LIGHTING);

    drawGLObject(sGroundPlane);

    glEnable(GL_LIGHTING);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}


//static void drawBox(int i) {
//    glDisable(GL_CULL_FACE);
//    glDisable(GL_DEPTH_TEST);
//    glEnable(GL_BLEND);
//    glBlendFunc(GL_ZERO, GL_SRC_COLOR);
//    glDisable(GL_LIGHTING);
//
//    drawGLObject(sBoxes[i]);
//
//    glEnable(GL_LIGHTING);
//    glDisable(GL_BLEND);
//    glEnable(GL_DEPTH_TEST);
//}


static void paintGL() {

    GLfloat object[] = {
            1.0, 0.0, 0.0,
            0.0, 1.0, 0.0,
            0.0, 0.0, 0.0
    };

    GLfloat colors[] = {
            0.0, 1.0, 0.0,
            1.0, 1.0, 0.0,
            0.0, 1.0, 1.0
    };

    glVertexPointer(3, GL_FLOAT, 0, object);
    glColorPointer(4, GL_FLOAT, 0, colors);

    glEnable(GL_BLEND);
    glLoadIdentity();
    glTranslatef(0, 0, -5);

    glDrawArrays(GL_TRIANGLES, 0, 3);

    glFlush();

}

// Called from the app framework.
void appInit() {
    glEnable(GL_NORMALIZE);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glShadeModel(GL_FLAT);

    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_LIGHT1);
    glEnable(GL_LIGHT2);

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);

    seedRandom(15);

    sGroundPlane = createGroundPlane();
    assert(sGroundPlane != NULL);

//    int i;
//    for (i = 0; i < NUMBOXES; i++) {
//        sBoxes[i] = createBox(i, i, i, 2, 2, 2);
//        assert(sBoxes[i] != NULL);
//    }
}


// Called from the app framework.
void appDeinit() {
    int a;
    for (a = 0; a < SUPERSHAPE_COUNT; ++a)
        freeGLObject(sSuperShapeObjects[a]);
    freeGLObject(sGroundPlane);
}


static void gluPerspective(GLfloat fovy, GLfloat aspect,
                           GLfloat zNear, GLfloat zFar) {
    GLfloat xmin, xmax, ymin, ymax;

    ymax = zNear * (GLfloat) tan(fovy * PI / 360);
    ymin = -ymax;
    xmin = ymin * aspect;
    xmax = ymax * aspect;

    glFrustumx((GLfixed) (xmin * 65536), (GLfixed) (xmax * 65536),
               (GLfixed) (ymin * 65536), (GLfixed) (ymax * 65536),
               (GLfixed) (zNear * 65536), (GLfixed) (zFar * 65536));
}


static void prepareFrame(int width, int height) {
    glViewport(0, 0, width, height);

    glClearColorx((GLfixed) (0.1f * 65536),
                  (GLfixed) (0.2f * 65536),
                  (GLfixed) (0.3f * 65536), 0x10000);
    glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45, (float) width / height, 0.5f, 150);

    glMatrixMode(GL_MODELVIEW);

    glLoadIdentity();
}


static void configureLightAndMaterial() {
    static GLfixed light0Position[] = {-0x40000, 0x10000, 0x10000, 0};
    static GLfixed light0Diffuse[] = {0x10000, 0x6666, 0, 0x10000};
    static GLfixed light1Position[] = {0x10000, -0x20000, -0x10000, 0};
    static GLfixed light1Diffuse[] = {0x11eb, 0x23d7, 0x5999, 0x10000};
    static GLfixed light2Position[] = {-0x10000, 0, -0x40000, 0};
    static GLfixed light2Diffuse[] = {0x11eb, 0x2b85, 0x23d7, 0x10000};
    static GLfixed materialSpecular[] = {0x10000, 0x10000, 0x10000, 0x10000};

    glLightxv(GL_LIGHT0, GL_POSITION, light0Position);
    glLightxv(GL_LIGHT0, GL_DIFFUSE, light0Diffuse);
    glLightxv(GL_LIGHT1, GL_POSITION, light1Position);
    glLightxv(GL_LIGHT1, GL_DIFFUSE, light1Diffuse);
    glLightxv(GL_LIGHT2, GL_POSITION, light2Position);
    glLightxv(GL_LIGHT2, GL_DIFFUSE, light2Diffuse);
    glMaterialxv(GL_FRONT_AND_BACK, GL_SPECULAR, materialSpecular);

    glMaterialx(GL_FRONT_AND_BACK, GL_SHININESS, 60 << 16);
    glEnable(GL_COLOR_MATERIAL);
}


/* Following gluLookAt implementation is adapted from the
 * Mesa 3D Graphics library. http://www.mesa3d.org
 */
static void gluLookAt(GLfloat eyex, GLfloat eyey, GLfloat eyez,
                      GLfloat centerx, GLfloat centery, GLfloat centerz,
                      GLfloat upx, GLfloat upy, GLfloat upz) {
    GLfloat m[16];
    GLfloat x[3], y[3], z[3];
    GLfloat mag;

    /* Make rotation matrix */

    /* Z vector */
    z[0] = eyex - centerx;
    z[1] = eyey - centery;
    z[2] = eyez - centerz;
    mag = (float) sqrt(z[0] * z[0] + z[1] * z[1] + z[2] * z[2]);
    if (mag) {            /* mpichler, 19950515 */
        z[0] /= mag;
        z[1] /= mag;
        z[2] /= mag;
    }

    /* Y vector */
    y[0] = upx;
    y[1] = upy;
    y[2] = upz;

    /* X vector = Y cross Z */
    x[0] = y[1] * z[2] - y[2] * z[1];
    x[1] = -y[0] * z[2] + y[2] * z[0];
    x[2] = y[0] * z[1] - y[1] * z[0];

    /* Recompute Y = Z cross X */
    y[0] = z[1] * x[2] - z[2] * x[1];
    y[1] = -z[0] * x[2] + z[2] * x[0];
    y[2] = z[0] * x[1] - z[1] * x[0];

    /* mpichler, 19950515 */
    /* cross product gives area of parallelogram, which is < 1.0 for
     * non-perpendicular unit-length vectors; so normalize x, y here
     */

    mag = (float) sqrt(x[0] * x[0] + x[1] * x[1] + x[2] * x[2]);
    if (mag) {
        x[0] /= mag;
        x[1] /= mag;
        x[2] /= mag;
    }

    mag = (float) sqrt(y[0] * y[0] + y[1] * y[1] + y[2] * y[2]);
    if (mag) {
        y[0] /= mag;
        y[1] /= mag;
        y[2] /= mag;
    }

#define M(row, col)  m[col*4+row]
    M(0, 0) = x[0];
    M(0, 1) = x[1];
    M(0, 2) = x[2];
    M(0, 3) = 0.0;
    M(1, 0) = y[0];
    M(1, 1) = y[1];
    M(1, 2) = y[2];
    M(1, 3) = 0.0;
    M(2, 0) = z[0];
    M(2, 1) = z[1];
    M(2, 2) = z[2];
    M(2, 3) = 0.0;
    M(3, 0) = 0.0;
    M(3, 1) = 0.0;
    M(3, 2) = 0.0;
    M(3, 3) = 1.0;
#undef M
    {
        int a;
        GLfixed fixedM[16];
        for (a = 0; a < 16; ++a)
            fixedM[a] = (GLfixed) (m[a] * 65536);
        glMultMatrixx(fixedM);
    }

    /* Translate Eye to Origin */
    glTranslatex((GLfixed) (-eyex * 65536),
                 (GLfixed) (-eyey * 65536),
                 (GLfixed) (-eyez * 65536));
}


// Called from the app framework.
/* The tick is current time in milliseconds, width and height
 * are the image dimensions to be rendered.
 */
void appRender(long tick, int width, int height) {
    if (sStartTick == 0)
        sStartTick = tick;
    if (!gAppAlive)
        return;

    // Actual tick value is "blurred" a little bit.
    sTick = (sTick + tick - sStartTick) >> 1;

    // Terminate application after running through the demonstration once.
    if (sTick >= RUN_LENGTH) {
        gAppAlive = 0;
        return;
    }

    // Prepare OpenGL ES for rendering of the frame.
    prepareFrame(width, height);

    // Update the camera position and set the lookat.
    gluLookAt(5, -5, 5, 0, 0, 0, 0, 0, 1);

    // Configure environment.
    configureLightAndMaterial();

    // Draw the reflection by drawing models with negated Z-axis.
//    glPushMatrix();
//    drawModels(-1);
//    glPopMatrix();

    // Blend the ground plane to the window.
    drawGroundPlane();

    // Create the box.
//    int i;
//    for (i = 0; i < NUMBOXES; i++) {
//        drawBox(i);
//    }

    paintGL();

}
