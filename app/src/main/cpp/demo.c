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
#define NUMBOXES 10
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

static int sCurrentCamTrack = 0;
static long sCurrentCamTrackStartTick = 0;
static long sNextCamTrackStartTick = 0x7fffffff;

static GLOBJECT *sSuperShapeObjects[SUPERSHAPE_COUNT] = {NULL};
static GLOBJECT *sGroundPlane = NULL;
static GLOBJECT *sBoxes[NUMBOXES];


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
    assert(object != NULL);

    glVertexPointer(object->vertexComponents, GL_FIXED,
                    0, object->vertexArray);
    glColorPointer(4, GL_UNSIGNED_BYTE, 0, object->colorArray);

    // Already done in initialization:
    //glEnableClientState(GL_VERTEX_ARRAY);
    //glEnableClientState(GL_COLOR_ARRAY);

    if (object->normalArray) {
        glNormalPointer(GL_FIXED, 0, object->normalArray);
        glEnableClientState(GL_NORMAL_ARRAY);
    }
    else
        glDisableClientState(GL_NORMAL_ARRAY);
    glDrawArrays(GL_TRIANGLES, 0, object->count);
}


static void vector3Sub(VECTOR3 *dest, VECTOR3 *v1, VECTOR3 *v2) {
    dest->x = v1->x - v2->x;
    dest->y = v1->y - v2->y;
    dest->z = v1->z - v2->z;
}


static void superShapeMap(VECTOR3 *point, float r1, float r2, float t, float p) {
    // sphere-mapping of supershape parameters
    point->x = (float) (cos(t) * cos(p) / r1 / r2);
    point->y = (float) (sin(t) * cos(p) / r1 / r2);
    point->z = (float) (sin(p) / r2);

}


static float ssFunc(const float t, const float *p) {
    return (float) (pow(pow(fabs(cos(p[0] * t / 4)) / p[1], p[4]) +
                        pow(fabs(sin(p[0] * t / 4)) / p[2], p[5]), 1 / p[3]));
}


// Creates and returns a supershape object.
// Based on Paul Bourke's POV-Ray implementation.
// http://astronomy.swin.edu.au/~pbourke/povray/supershape/
static GLOBJECT *createSuperShape(const float *params) {
    const int resol1 = (int) params[SUPERSHAPE_PARAMS - 3];
    const int resol2 = (int) params[SUPERSHAPE_PARAMS - 2];
    // latitude 0 to pi/2 for no mirrored bottom
    // (latitudeBegin==0 for -pi/2 to pi/2 originally)
    const int latitudeBegin = resol2 / 4;
    const int latitudeEnd = resol2 / 2;    // non-inclusive
    const int longitudeCount = resol1;
    const int latitudeCount = latitudeEnd - latitudeBegin;
    const long triangleCount = longitudeCount * latitudeCount * 2;
    const long vertices = triangleCount * 3;
    GLOBJECT *result;
    float baseColor[3];
    int a, longitude, latitude;
    long currentVertex, currentQuad;

    result = newGLObject(vertices, 3, 1);
    if (result == NULL)
        return NULL;

    for (a = 0; a < 3; ++a)
        baseColor[a] = ((randomUInt() % 155) + 100) / 255.f;

    currentQuad = 0;
    currentVertex = 0;

    // longitude -pi to pi
    for (longitude = 0; longitude < longitudeCount; ++longitude) {

        // latitude 0 to pi/2
        for (latitude = latitudeBegin; latitude < latitudeEnd; ++latitude) {
            float t1 = -PI + longitude * 2 * PI / resol1;
            float t2 = -PI + (longitude + 1) * 2 * PI / resol1;
            float p1 = -PI / 2 + latitude * 2 * PI / resol2;
            float p2 = -PI / 2 + (latitude + 1) * 2 * PI / resol2;
            float r0, r1, r2, r3;

            r0 = ssFunc(t1, params);
            r1 = ssFunc(p1, &params[6]);
            r2 = ssFunc(t2, params);
            r3 = ssFunc(p2, &params[6]);

            if (r0 != 0 && r1 != 0 && r2 != 0 && r3 != 0) {
                VECTOR3 pa, pb, pc, pd;
                VECTOR3 v1, v2, n;
                float ca;
                int i;
                //float lenSq, invLenSq;

                superShapeMap(&pa, r0, r1, t1, p1);
                superShapeMap(&pb, r2, r1, t2, p1);
                superShapeMap(&pc, r2, r3, t2, p2);
                superShapeMap(&pd, r0, r3, t1, p2);

                // kludge to set lower edge of the object to fixed level
                if (latitude == latitudeBegin + 1)
                    pa.z = pb.z = 0;

                vector3Sub(&v1, &pb, &pa);
                vector3Sub(&v2, &pd, &pa);

                // Calculate normal with cross product.
                /*   i    j    k      i    j
                 * v1.x v1.y v1.z | v1.x v1.y
                 * v2.x v2.y v2.z | v2.x v2.y
                 */

                n.x = v1.y * v2.z - v1.z * v2.y;
                n.y = v1.z * v2.x - v1.x * v2.z;
                n.z = v1.x * v2.y - v1.y * v2.x;

                /* Pre-normalization of the normals is disabled here because
                 * they will be normalized anyway later due to automatic
                 * normalization (GL_NORMALIZE). It is enabled because the
                 * objects are scaled with glScale.
                 */
                /*
                lenSq = n.x * n.x + n.y * n.y + n.z * n.z;
                invLenSq = (float)(1 / sqrt(lenSq));
                n.x *= invLenSq;
                n.y *= invLenSq;
                n.z *= invLenSq;
                */

                ca = pa.z + 0.5f;

                for (i = (int) (currentVertex * 3);
                     i < (currentVertex + 6) * 3;
                     i += 3) {
                    result->normalArray[i] = (GLfixed) FIXED(n.x);
                    result->normalArray[i + 1] = (GLfixed) FIXED(n.y);
                    result->normalArray[i + 2] = (GLfixed) FIXED(n.z);
                }
                for (i = (int) (currentVertex * 4);
                     i < (currentVertex + 6) * 4;
                     i += 4) {
                    int b, color[3];
                    for (b = 0; b < 3; ++b) {
                        color[b] = (int) (ca * baseColor[b] * 255);
                        if (color[b] > 255) color[b] = 255;
                    }
                    result->colorArray[i] = (GLubyte) color[0];
                    result->colorArray[i + 1] = (GLubyte) color[1];
                    result->colorArray[i + 2] = (GLubyte) color[2];
                    result->colorArray[i + 3] = 0;
                }
                result->vertexArray[currentVertex * 3] = (GLfixed) FIXED(pa.x);
                result->vertexArray[currentVertex * 3 + 1] = (GLfixed) FIXED(pa.y);
                result->vertexArray[currentVertex * 3 + 2] = (GLfixed) FIXED(pa.z);
                ++currentVertex;
                result->vertexArray[currentVertex * 3] = (GLfixed) FIXED(pb.x);
                result->vertexArray[currentVertex * 3 + 1] = (GLfixed) FIXED(pb.y);
                result->vertexArray[currentVertex * 3 + 2] = (GLfixed) FIXED(pb.z);
                ++currentVertex;
                result->vertexArray[currentVertex * 3] = (GLfixed) FIXED(pd.x);
                result->vertexArray[currentVertex * 3 + 1] = (GLfixed) FIXED(pd.y);
                result->vertexArray[currentVertex * 3 + 2] = (GLfixed) FIXED(pd.z);
                ++currentVertex;
                result->vertexArray[currentVertex * 3] = (GLfixed) FIXED(pb.x);
                result->vertexArray[currentVertex * 3 + 1] = (GLfixed) FIXED(pb.y);
                result->vertexArray[currentVertex * 3 + 2] = (GLfixed) FIXED(pb.z);
                ++currentVertex;
                result->vertexArray[currentVertex * 3] = (GLfixed) FIXED(pc.x);
                result->vertexArray[currentVertex * 3 + 1] = (GLfixed) FIXED(pc.y);
                result->vertexArray[currentVertex * 3 + 2] = (GLfixed) FIXED(pc.z);
                ++currentVertex;
                result->vertexArray[currentVertex * 3] = (GLfixed) FIXED(pd.x);
                result->vertexArray[currentVertex * 3 + 1] = (GLfixed) FIXED(pd.y);
                result->vertexArray[currentVertex * 3 + 2] = (GLfixed) FIXED(pd.z);
                ++currentVertex;
            } // r0 && r1 && r2 && r3
            ++currentQuad;
        } // latitude
    } // longitude

    // Set number of vertices in object to the actual amount created.
    result->count = (GLsizei) currentVertex;

    return result;
}

static void assignVertex(int pi, int m, GLOBJECT *result, long p[][3]) {
    int i;
    GLubyte color;
    for (i = 0; i < 3; i++) {
        __android_log_print(ANDROID_LOG_DEBUG, "vf", "vertex[%d] = %d", m * 3 + i,
                            (int) p[pi - 1][i]);
        result->vertexArray[m * 3 + i] = (GLfixed) p[pi - 1][i];

        int j;
        for (j = 0; j < 4; j++) {
            color = (GLubyte) (j == 3 ? 0 : (GLubyte) ((randomUInt() & 0x5f) + 10 * m + j));
            result->colorArray[(m * 3 + i) * 4 + j] = color;
//            __android_log_print(ANDROID_LOG_DEBUG, "vf",
//                                "color[%d] = %d", (m * 3 + i) * 4 + j, color);
        }
    }
}

static void assignNormal(int pi, GLOBJECT *result) {
    int i;
//    __android_log_print(ANDROID_LOG_DEBUG, "vf", "assign normal to triangle %d", pi/3+1);
    VECTOR3 r1, r2, r3, v1, v2, n;

    r1.x = result->vertexArray[pi * 3];
    r1.y = result->vertexArray[pi * 3 + 1];
    r1.z = result->vertexArray[pi * 3 + 2];

    r2.x = result->vertexArray[pi * 3 + 3];
    r2.y = result->vertexArray[pi * 3 + 4];
    r2.z = result->vertexArray[pi * 3 + 5];

    r3.x = result->vertexArray[pi * 3 + 6];
    r3.y = result->vertexArray[pi * 3 + 7];
    r3.z = result->vertexArray[pi * 3 + 8];

//    __android_log_print(ANDROID_LOG_DEBUG, "vf", "r1 = (%2.2f, %2.2f, %2.2f)",
//                        r1.x, r1.y, r1.z);
//    __android_log_print(ANDROID_LOG_DEBUG, "vf", "r2 = (%2.2f, %2.2f, %2.2f)",
//                        r2.x, r2.y, r2.z);
//    __android_log_print(ANDROID_LOG_DEBUG, "vf", "r3 = (%2.2f, %2.2f, %2.2f)",
//                        r3.x, r3.y, r3.z);

    vector3Sub(&v1, &r2, &r1);
    vector3Sub(&v2, &r3, &r2);

//    __android_log_print(ANDROID_LOG_DEBUG, "vf", "v1 = (%2.2f, %2.2f, %2.2f)",
//                        v1.x, v1.y, v1.z);
//    __android_log_print(ANDROID_LOG_DEBUG, "vf", "v2 = (%2.2f, %2.2f, %2.2f)",
//                        v2.x, v2.y, v2.z);
//
//    __android_log_print(ANDROID_LOG_DEBUG, "vf", "n.x = %2.2f * %2.2f - %2.2f * %2.2f",
//                        v1.y, v2.z, v1.z, v2.y);
//    __android_log_print(ANDROID_LOG_DEBUG, "vf", "n.y = %2.2f * %2.2f - %2.2f * %2.2f",
//                        v1.z, v2.x, v1.x, v2.z);
//    __android_log_print(ANDROID_LOG_DEBUG, "vf", "n.z = %2.2f * %2.2f - %2.2f * %2.2f",
//                        v1.x, v2.y, v1.y, v2.x);

    n.x = v1.y * v2.z - v1.z * v2.y;
    n.y = v1.z * v2.x - v1.x * v2.z;
    n.z = v1.x * v2.y - v1.y * v2.x;

    float mag = (float) sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
    if (mag) {
        n.x /= mag;
        n.y /= mag;
        n.z /= mag;
    }

    float nvector[3] = {n.x, n.y, n.z};

    for (i = 0; i < 3; i++) {
        result->normalArray[pi * 3 + i] = (GLfixed) nvector[i];
    }
    __android_log_print(ANDROID_LOG_DEBUG, "vf", "normal for triangle %d = (%2.2f, %2.2f, %2.2f)",
                        pi / 3 + 1, nvector[0], nvector[1], nvector[2]);
}

static GLOBJECT *createBox(long x, long y, long z, long w, long l, long h) {
    const long triangleCount = 12;
    const long vertices = triangleCount * 3;
    GLOBJECT *result;

    long p[8][3] = {{x,     y,     z},
                    {x + w, y,     z},
                    {x + w, y + l, z},
                    {x,     y + l, z},
                    {x,     y,     z + h},
                    {x + w, y,     z + h},
                    {x + w, y + l, z + h},
                    {x,     y + l, z + h}};

    result = newGLObject(vertices, 3, 1);

    assignVertex(1, 0, result, p);
    assignVertex(2, 1, result, p);
    assignVertex(6, 2, result, p);
    assignNormal(0, result);

    assignVertex(6, 3, result, p);
    assignVertex(5, 4, result, p);
    assignVertex(1, 5, result, p);
    assignNormal(3, result);

    assignVertex(2, 6, result, p);
    assignVertex(3, 7, result, p);
    assignVertex(7, 8, result, p);
    assignNormal(6, result);

    assignVertex(7, 9, result, p);
    assignVertex(6, 10, result, p);
    assignVertex(2, 11, result, p);
    assignNormal(9, result);

    assignVertex(3, 12, result, p);
    assignVertex(4, 13, result, p);
    assignVertex(8, 14, result, p);
    assignNormal(12, result);

    assignVertex(8, 15, result, p);
    assignVertex(7, 16, result, p);
    assignVertex(3, 17, result, p);
    assignNormal(15, result);

    assignVertex(4, 18, result, p);
    assignVertex(1, 19, result, p);
    assignVertex(5, 20, result, p);
    assignNormal(18, result);

    assignVertex(5, 21, result, p);
    assignVertex(8, 22, result, p);
    assignVertex(4, 23, result, p);
    assignNormal(21, result);

    assignVertex(5, 24, result, p);
    assignVertex(6, 25, result, p);
    assignVertex(7, 26, result, p);
    assignNormal(24, result);

    assignVertex(7, 27, result, p);
    assignVertex(8, 28, result, p);
    assignVertex(5, 29, result, p);
    assignNormal(27, result);

    assignVertex(2, 30, result, p);
    assignVertex(1, 31, result, p);
    assignVertex(4, 32, result, p);
    assignNormal(30, result);

    assignVertex(4, 33, result, p);
    assignVertex(3, 34, result, p);
    assignVertex(2, 35, result, p);
    assignNormal(33, result);

    return result;

}

static GLOBJECT *createGroundPlane() {
    const int scale = 1;
    const int yBegin = -3, yEnd = 3;    // ends are non-inclusive
    const int xBegin = -3, xEnd = 3;
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


static void drawBox(int i) {
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ZERO, GL_SRC_COLOR);
    glDisable(GL_LIGHTING);

    drawGLObject(sBoxes[i]);

    glEnable(GL_LIGHTING);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}


static void drawFadeQuad() {
    static const GLfixed quadVertices[] = {
            -0x10000, -0x10000,
            0x10000, -0x10000,
            -0x10000, 0x10000,
            0x10000, -0x10000,
            0x10000, 0x10000,
            -0x10000, 0x10000
    };

    const int beginFade = (const int) (sTick - sCurrentCamTrackStartTick);
    const int endFade = (const int) (sNextCamTrackStartTick - sTick);
    const int minFade = beginFade < endFade ? beginFade : endFade;

    if (minFade < 1024) {
        const GLfixed fadeColor = minFade << 6;
        glColor4x(fadeColor, fadeColor, fadeColor, 0);

        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_ZERO, GL_SRC_COLOR);
        glDisable(GL_LIGHTING);

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();

        glDisableClientState(GL_COLOR_ARRAY);
        glDisableClientState(GL_NORMAL_ARRAY);
        glVertexPointer(2, GL_FIXED, 0, quadVertices);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        glEnableClientState(GL_COLOR_ARRAY);

        glMatrixMode(GL_MODELVIEW);

        glEnable(GL_LIGHTING);
        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
    }
}


// Called from the app framework.
void appInit() {
    int a;

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

    for (a = 0; a < SUPERSHAPE_COUNT; ++a) {
        sSuperShapeObjects[a] = createSuperShape(sSuperShapeParams[a]);
        assert(sSuperShapeObjects[a] != NULL);
    }

    sGroundPlane = createGroundPlane();
    assert(sGroundPlane != NULL);

    int i;
    for (i = 0; i < NUMBOXES; i++) {
        sBoxes[i] = createBox(10 * i, i, i, 10, 20, 10);
        assert(sBoxes[i] != NULL);
    }
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


//static void drawModels(float zScale) {
//    const int translationScale = 9;
//    int x, y;
//
//    seedRandom(9);
//
//    glScalex(1 << 16, 1 << 16, (GLfixed) (zScale * 65536));
//
//    for (y = -5; y <= 5; ++y) {
//        for (x = -5; x <= 5; ++x) {
//            float buildingScale;
//            GLfixed fixedScale;
//
//            int curShape = (int) (randomUInt() % SUPERSHAPE_COUNT);
//            buildingScale = sSuperShapeParams[curShape][SUPERSHAPE_PARAMS - 1];
//            fixedScale = (GLfixed) (buildingScale * 65536);
//
//            glPushMatrix();
//            glTranslatex((x * translationScale) * 65536,
//                         (y * translationScale) * 65536,
//                         0);
//            glRotatex((GLfixed) ((randomUInt() % 360) << 16), 0, 0, 1 << 16);
//            glScalex(fixedScale, fixedScale, fixedScale);
//
//            drawGLObject(sSuperShapeObjects[curShape]);
//            glPopMatrix();
//        }
//    }
//
//    for (x = -2; x <= 2; ++x) {
//        const int shipScale100 = translationScale * 500;
//        const int offs100 = (const int) (x * shipScale100 + (sTick % shipScale100));
//        float offs = offs100 * 0.01f;
//        GLfixed fixedOffs = (GLfixed) (offs * 65536);
//        glPushMatrix();
//        glTranslatex(fixedOffs, -4 * 65536, 2 << 16);
//        drawGLObject(sSuperShapeObjects[SUPERSHAPE_COUNT - 1]);
//        glPopMatrix();
//        glPushMatrix();
//        glTranslatex(-4 * 65536, fixedOffs, 4 << 16);
//        glRotatex(90 << 16, 0, 0, 1 << 16);
//        drawGLObject(sSuperShapeObjects[SUPERSHAPE_COUNT - 1]);
//        glPopMatrix();
//    }
//}


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


static void camTrack() {
    float lerp[5];
    float eX, eY, eZ, cX, cY, cZ;
    float trackPos;
    CAMTRACK *cam;
    long currentCamTick;
    int a;

    sCurrentCamTrack = 6;

    if (sNextCamTrackStartTick <= sTick) {
//        ++sCurrentCamTrack;
        sCurrentCamTrackStartTick = sNextCamTrackStartTick;
    }
    sNextCamTrackStartTick = sCurrentCamTrackStartTick +
                             sCamTracks[sCurrentCamTrack].len * CAMTRACK_LEN;

    cam = &sCamTracks[sCurrentCamTrack];
    currentCamTick = sTick - sCurrentCamTrackStartTick;
    trackPos = (float) currentCamTick / (CAMTRACK_LEN * cam->len);

    for (a = 0; a < 5; ++a)
        lerp[a] = (cam->src[a] + cam->dest[a] * trackPos) * 0.01f;

    if (cam->dist) {
        float dist = cam->dist * 0.1f;
        cX = lerp[0];
        cY = lerp[1];
        cZ = lerp[2];
        eX = cX - (float) cos(lerp[3]) * dist;
        eY = cY - (float) sin(lerp[3]) * dist;
        eZ = cZ - lerp[4];
    }
    else {
        eX = lerp[0];
        eY = lerp[1];
        eZ = lerp[2];
        cX = eX + (float) cos(lerp[3]);
        cY = eY + (float) sin(lerp[3]);
        cZ = eZ + lerp[4];
    }
    gluLookAt(eX, eY, eZ, cX, cY, cZ, 0, 0, 1);
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
    camTrack();

    // Configure environment.
    configureLightAndMaterial();

    // Draw the reflection by drawing models with negated Z-axis.
    glPushMatrix();
//    drawModels(-1);
    glPopMatrix();

    // Blend the ground plane to the window.
    drawGroundPlane();

    // Create the box.
    int i;
    for (i = 0; i < NUMBOXES; i++) {
        drawBox(i);
    }

    // Draw all the models normally.
//    drawModels(1.0);

    // Draw fade quad over whole window (when changing cameras).
    drawFadeQuad();

}
