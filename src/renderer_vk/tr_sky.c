/*
===========================================================================

Return to Castle Wolfenstein multiplayer GPL Source Code
Copyright (C) 1999-2010 id Software LLC, a ZeniMax Media company. 

This file is part of the Return to Castle Wolfenstein multiplayer GPL Source Code (RTCW MP Source Code).  

RTCW MP Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

RTCW MP Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with RTCW MP Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the RTCW MP Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the RTCW MP Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/

// tr_sky.c
#include "tr_local.h"

#define SKY_SUBDIVISIONS        8
#define HALF_SKY_SUBDIVISIONS   ( SKY_SUBDIVISIONS / 2 )

static float s_cloudTexCoords[6][SKY_SUBDIVISIONS + 1][SKY_SUBDIVISIONS + 1][2];
static float s_cloudTexP[6][SKY_SUBDIVISIONS + 1][SKY_SUBDIVISIONS + 1];

/*
===================================================================================

POLYGON TO BOX SIDE PROJECTION

===================================================================================
*/

static vec3_t sky_clip[6] =
{
	{1,1,0},
	{1,-1,0},
	{0,-1,1},
	{0,1,1},
	{1,0,1},
	{-1,0,1}
};

static float sky_mins[2][6], sky_maxs[2][6];
static float sky_min, sky_max;

/*
================
AddSkyPolygon
================
*/
static void AddSkyPolygon( int nump, vec3_t vecs ) {
	int i,j;
	vec3_t v, av;
	float s, t, dv;
	int axis;
	float   *vp;
	// s = [0]/[2], t = [1]/[2]
	static int vec_to_st[6][3] =
	{
		{-2,3,1},
		{2,3,-1},

		{1,3,2},
		{-1,3,-2},

		{-2,-1,3},
		{-2,1,-3}

		//	{-1,2,3},
		//	{1,2,-3}
	};

	// decide which face it maps to
	VectorCopy( vec3_origin, v );
	for ( i = 0, vp = vecs ; i < nump ; i++, vp += 3 )
	{
		VectorAdd( vp, v, v );
	}
	av[0] = fabs( v[0] );
	av[1] = fabs( v[1] );
	av[2] = fabs( v[2] );
	if ( av[0] > av[1] && av[0] > av[2] ) {
		if ( v[0] < 0 ) {
			axis = 1;
		} else {
			axis = 0;
		}
	} else if ( av[1] > av[2] && av[1] > av[0] )     {
		if ( v[1] < 0 ) {
			axis = 3;
		} else {
			axis = 2;
		}
	} else
	{
		if ( v[2] < 0 ) {
			axis = 5;
		} else {
			axis = 4;
		}
	}

	// project new texture coords
	for ( i = 0 ; i < nump ; i++, vecs += 3 )
	{
		j = vec_to_st[axis][2];
		if ( j > 0 ) {
			dv = vecs[j - 1];
		} else {
			dv = -vecs[-j - 1];
		}
		if ( dv < 0.001 ) {
			continue;   // don't divide by zero
		}
		j = vec_to_st[axis][0];
		if ( j < 0 ) {
			s = -vecs[-j - 1] / dv;
		} else {
			s = vecs[j - 1] / dv;
		}
		j = vec_to_st[axis][1];
		if ( j < 0 ) {
			t = -vecs[-j - 1] / dv;
		} else {
			t = vecs[j - 1] / dv;
		}

		if ( s < sky_mins[0][axis] ) {
			sky_mins[0][axis] = s;
		}
		if ( t < sky_mins[1][axis] ) {
			sky_mins[1][axis] = t;
		}
		if ( s > sky_maxs[0][axis] ) {
			sky_maxs[0][axis] = s;
		}
		if ( t > sky_maxs[1][axis] ) {
			sky_maxs[1][axis] = t;
		}
	}
}

#define ON_EPSILON      0.1f            // point on plane side epsilon
#define MAX_CLIP_VERTS  64
/*
================
ClipSkyPolygon
================
*/
static void ClipSkyPolygon( int nump, vec3_t vecs, int stage ) {
	float   *norm;
	float   *v;
	qboolean front, back;
	float d, e;
	float dists[MAX_CLIP_VERTS];
	int sides[MAX_CLIP_VERTS];
	vec3_t newv[2][MAX_CLIP_VERTS];
	int newc[2];
	int i, j;

	if ( nump > MAX_CLIP_VERTS - 2 ) {
		ri.Error( ERR_DROP, "ClipSkyPolygon: MAX_CLIP_VERTS" );
	}
	if ( stage == 6 ) { // fully clipped, so draw it
		AddSkyPolygon( nump, vecs );
		return;
	}

	front = back = qfalse;
	norm = sky_clip[stage];
	for ( i = 0, v = vecs ; i < nump ; i++, v += 3 )
	{
		d = DotProduct( v, norm );
		if ( d > ON_EPSILON ) {
			front = qtrue;
			sides[i] = SIDE_FRONT;
		} else if ( d < -ON_EPSILON )     {
			back = qtrue;
			sides[i] = SIDE_BACK;
		} else {
			sides[i] = SIDE_ON;
		}
		dists[i] = d;
	}

	if ( !front || !back ) { // not clipped
		ClipSkyPolygon( nump, vecs, stage + 1 );
		return;
	}

	// clip it
	sides[i] = sides[0];
	dists[i] = dists[0];
	VectorCopy( vecs, ( vecs + ( i * 3 ) ) );
	newc[0] = newc[1] = 0;

	for ( i = 0, v = vecs ; i < nump ; i++, v += 3 )
	{
		switch ( sides[i] )
		{
		case SIDE_FRONT:
			VectorCopy( v, newv[0][newc[0]] );
			newc[0]++;
			break;
		case SIDE_BACK:
			VectorCopy( v, newv[1][newc[1]] );
			newc[1]++;
			break;
		case SIDE_ON:
			VectorCopy( v, newv[0][newc[0]] );
			newc[0]++;
			VectorCopy( v, newv[1][newc[1]] );
			newc[1]++;
			break;
		}

		if ( sides[i] == SIDE_ON || sides[i + 1] == SIDE_ON || sides[i + 1] == sides[i] ) {
			continue;
		}

		d = dists[i] / ( dists[i] - dists[i + 1] );
		for ( j = 0 ; j < 3 ; j++ )
		{
			e = v[j] + d * ( v[j + 3] - v[j] );
			newv[0][newc[0]][j] = e;
			newv[1][newc[1]][j] = e;
		}
		newc[0]++;
		newc[1]++;
	}

	// continue
	ClipSkyPolygon( newc[0], newv[0][0], stage + 1 );
	ClipSkyPolygon( newc[1], newv[1][0], stage + 1 );
}

/*
==============
ClearSkyBox
==============
*/
static void ClearSkyBox( void ) {
	int i;

	for ( i = 0 ; i < 6 ; i++ ) {
		sky_mins[0][i] = sky_mins[1][i] = 9999;
		sky_maxs[0][i] = sky_maxs[1][i] = -9999;
	}
}

/*
================
RB_ClipSkyPolygons
================
*/
void RB_ClipSkyPolygons( shaderCommands_t *input ) {
	vec3_t p[5];        // need one extra point for clipping
	int i, j;

	ClearSkyBox();

	for ( i = 0; i < input->numIndexes; i += 3 )
	{
		for ( j = 0 ; j < 3 ; j++ )
		{
			VectorSubtract( input->xyz[input->indexes[i + j]],
							backEnd.viewParms.or.origin,
							p[j] );
		}
		ClipSkyPolygon( 3, p[0], 0 );
	}
}

/*
===================================================================================

CLOUD VERTEX GENERATION

===================================================================================
*/

/*
** MakeSkyVec
**
** Parms: s, t range from -1 to 1
*/
static void MakeSkyVec( float s, float t, int axis, float outSt[2], vec3_t outXYZ ) {
	// 1 = s, 2 = t, 3 = 2048
	static int st_to_vec[6][3] =
	{
		{3,-1,2},
		{-3,1,2},

		{1,3,2},
		{-1,-3,2},

		{-2,-1,3},      // 0 degrees yaw, look straight up
		{2,-1,-3}       // look straight down
	};

	vec3_t b;
	int j, k;
	float boxSize;

// JPW NERVE swiped from Sherman SP fix
//	if(glfogNum > FOG_NONE && glfogsettings[FOG_CURRENT].mode == GL_EXP) {
	if ( glfogsettings[FOG_SKY].registered ) {     // (SA) trying this...
///		boxSize = backEnd.viewParms.zFar / 1.75;		// div sqrt(3)
//		boxSize = glfogsettings[FOG_CURRENT].end / 1.75;
		boxSize = glfogsettings[FOG_SKY].end;       // (SA) trying this...
// jpw
	} else {
		boxSize = backEnd.viewParms.zFar / 1.75;        // div sqrt(3)

	}
// JPW NERVE swiped from Sherman
	// make sure the sky is not near clipped
	if ( boxSize < r_znear->value * 2.0 ) {
		boxSize = r_znear->value * 2.0;
	}
// jpw
	b[0] = s * boxSize;
	b[1] = t * boxSize;
	b[2] = boxSize;

	for ( j = 0 ; j < 3 ; j++ )
	{
		k = st_to_vec[axis][j];
		if ( k < 0 ) {
			outXYZ[j] = -b[-k - 1];
		} else
		{
			outXYZ[j] = b[k - 1];
		}
	}

	// avoid bilerp seam
	s = ( s + 1 ) * 0.5;
	t = ( t + 1 ) * 0.5;
	if ( s < sky_min ) {
		s = sky_min;
	} else if ( s > sky_max )     {
		s = sky_max;
	}

	if ( t < sky_min ) {
		t = sky_min;
	} else if ( t > sky_max )     {
		t = sky_max;
	}

	t = 1.0 - t;


	if ( outSt ) {
		outSt[0] = s;
		outSt[1] = t;
	}
}

static int sky_texorder[6] = {0,2,1,3,4,5};
static vec3_t s_skyPoints[SKY_SUBDIVISIONS + 1][SKY_SUBDIVISIONS + 1];
static float s_skyTexCoords[SKY_SUBDIVISIONS + 1][SKY_SUBDIVISIONS + 1][2];

static void DrawSkySide( struct image_s *image, const int mins[2], const int maxs[2] ) {
	//@TODO
}

static void DrawSkySideInner( struct image_s *image, const int mins[2], const int maxs[2] ) {
	//@TODO
}

static void DrawSkyBox( shader_t *shader ) {
	int i;

	memset( s_skyTexCoords, 0, sizeof( s_skyTexCoords ) );

	sky_min = 0;
	sky_max = 1;

	for ( i = 0 ; i < 6 ; i++ )
	{
		int sky_mins_subd[2], sky_maxs_subd[2];
		int s, t;

		sky_mins[0][i] = floor( sky_mins[0][i] * HALF_SKY_SUBDIVISIONS ) / HALF_SKY_SUBDIVISIONS;
		sky_mins[1][i] = floor( sky_mins[1][i] * HALF_SKY_SUBDIVISIONS ) / HALF_SKY_SUBDIVISIONS;
		sky_maxs[0][i] = ceil( sky_maxs[0][i] * HALF_SKY_SUBDIVISIONS ) / HALF_SKY_SUBDIVISIONS;
		sky_maxs[1][i] = ceil( sky_maxs[1][i] * HALF_SKY_SUBDIVISIONS ) / HALF_SKY_SUBDIVISIONS;

		if ( ( sky_mins[0][i] >= sky_maxs[0][i] ) ||
			 ( sky_mins[1][i] >= sky_maxs[1][i] ) ) {
			continue;
		}

		sky_mins_subd[0] = sky_mins[0][i] * HALF_SKY_SUBDIVISIONS;
		sky_mins_subd[1] = sky_mins[1][i] * HALF_SKY_SUBDIVISIONS;
		sky_maxs_subd[0] = sky_maxs[0][i] * HALF_SKY_SUBDIVISIONS;
		sky_maxs_subd[1] = sky_maxs[1][i] * HALF_SKY_SUBDIVISIONS;

		if ( sky_mins_subd[0] < -HALF_SKY_SUBDIVISIONS ) {
			sky_mins_subd[0] = -HALF_SKY_SUBDIVISIONS;
		} else if ( sky_mins_subd[0] > HALF_SKY_SUBDIVISIONS ) {
			sky_mins_subd[0] = HALF_SKY_SUBDIVISIONS;
		}
		if ( sky_mins_subd[1] < -HALF_SKY_SUBDIVISIONS ) {
			sky_mins_subd[1] = -HALF_SKY_SUBDIVISIONS;
		} else if ( sky_mins_subd[1] > HALF_SKY_SUBDIVISIONS ) {
			sky_mins_subd[1] = HALF_SKY_SUBDIVISIONS;
		}

		if ( sky_maxs_subd[0] < -HALF_SKY_SUBDIVISIONS ) {
			sky_maxs_subd[0] = -HALF_SKY_SUBDIVISIONS;
		} else if ( sky_maxs_subd[0] > HALF_SKY_SUBDIVISIONS ) {
			sky_maxs_subd[0] = HALF_SKY_SUBDIVISIONS;
		}
		if ( sky_maxs_subd[1] < -HALF_SKY_SUBDIVISIONS ) {
			sky_maxs_subd[1] = -HALF_SKY_SUBDIVISIONS;
		} else if ( sky_maxs_subd[1] > HALF_SKY_SUBDIVISIONS ) {
			sky_maxs_subd[1] = HALF_SKY_SUBDIVISIONS;
		}

		//
		// iterate through the subdivisions
		//
		for ( t = sky_mins_subd[1] + HALF_SKY_SUBDIVISIONS; t <= sky_maxs_subd[1] + HALF_SKY_SUBDIVISIONS; t++ )
		{
			for ( s = sky_mins_subd[0] + HALF_SKY_SUBDIVISIONS; s <= sky_maxs_subd[0] + HALF_SKY_SUBDIVISIONS; s++ )
			{
				MakeSkyVec( ( s - HALF_SKY_SUBDIVISIONS ) / ( float ) HALF_SKY_SUBDIVISIONS,
							( t - HALF_SKY_SUBDIVISIONS ) / ( float ) HALF_SKY_SUBDIVISIONS,
							i,
							s_skyTexCoords[t][s],
							s_skyPoints[t][s] );
			}
		}

		DrawSkySide( shader->sky.outerbox[sky_texorder[i]],
					 sky_mins_subd,
					 sky_maxs_subd );
	}

}


static void DrawSkyBoxInner( shader_t *shader ) {
	int i;

	memset( s_skyTexCoords, 0, sizeof( s_skyTexCoords ) );

	for ( i = 0 ; i < 6 ; i++ )
	{
		int sky_mins_subd[2], sky_maxs_subd[2];
		int s, t;

		sky_mins[0][i] = floor( sky_mins[0][i] * HALF_SKY_SUBDIVISIONS ) / HALF_SKY_SUBDIVISIONS;
		sky_mins[1][i] = floor( sky_mins[1][i] * HALF_SKY_SUBDIVISIONS ) / HALF_SKY_SUBDIVISIONS;
		sky_maxs[0][i] = ceil( sky_maxs[0][i] * HALF_SKY_SUBDIVISIONS ) / HALF_SKY_SUBDIVISIONS;
		sky_maxs[1][i] = ceil( sky_maxs[1][i] * HALF_SKY_SUBDIVISIONS ) / HALF_SKY_SUBDIVISIONS;

		if ( ( sky_mins[0][i] >= sky_maxs[0][i] ) ||
			 ( sky_mins[1][i] >= sky_maxs[1][i] ) ) {
			continue;
		}

		sky_mins_subd[0] = sky_mins[0][i] * HALF_SKY_SUBDIVISIONS;
		sky_mins_subd[1] = sky_mins[1][i] * HALF_SKY_SUBDIVISIONS;
		sky_maxs_subd[0] = sky_maxs[0][i] * HALF_SKY_SUBDIVISIONS;
		sky_maxs_subd[1] = sky_maxs[1][i] * HALF_SKY_SUBDIVISIONS;

		if ( sky_mins_subd[0] < -HALF_SKY_SUBDIVISIONS ) {
			sky_mins_subd[0] = -HALF_SKY_SUBDIVISIONS;
		} else if ( sky_mins_subd[0] > HALF_SKY_SUBDIVISIONS ) {
			sky_mins_subd[0] = HALF_SKY_SUBDIVISIONS;
		}
		if ( sky_mins_subd[1] < -HALF_SKY_SUBDIVISIONS ) {
			sky_mins_subd[1] = -HALF_SKY_SUBDIVISIONS;
		} else if ( sky_mins_subd[1] > HALF_SKY_SUBDIVISIONS ) {
			sky_mins_subd[1] = HALF_SKY_SUBDIVISIONS;
		}

		if ( sky_maxs_subd[0] < -HALF_SKY_SUBDIVISIONS ) {
			sky_maxs_subd[0] = -HALF_SKY_SUBDIVISIONS;
		} else if ( sky_maxs_subd[0] > HALF_SKY_SUBDIVISIONS ) {
			sky_maxs_subd[0] = HALF_SKY_SUBDIVISIONS;
		}
		if ( sky_maxs_subd[1] < -HALF_SKY_SUBDIVISIONS ) {
			sky_maxs_subd[1] = -HALF_SKY_SUBDIVISIONS;
		} else if ( sky_maxs_subd[1] > HALF_SKY_SUBDIVISIONS ) {
			sky_maxs_subd[1] = HALF_SKY_SUBDIVISIONS;
		}

		//
		// iterate through the subdivisions
		//
		for ( t = sky_mins_subd[1] + HALF_SKY_SUBDIVISIONS; t <= sky_maxs_subd[1] + HALF_SKY_SUBDIVISIONS; t++ )
		{
			for ( s = sky_mins_subd[0] + HALF_SKY_SUBDIVISIONS; s <= sky_maxs_subd[0] + HALF_SKY_SUBDIVISIONS; s++ )
			{
				MakeSkyVec( ( s - HALF_SKY_SUBDIVISIONS ) / ( float ) HALF_SKY_SUBDIVISIONS,
							( t - HALF_SKY_SUBDIVISIONS ) / ( float ) HALF_SKY_SUBDIVISIONS,
							i,
							s_skyTexCoords[t][s],
							s_skyPoints[t][s] );
			}
		}

		DrawSkySideInner( shader->sky.innerbox[sky_texorder[i]],
						  sky_mins_subd,
						  sky_maxs_subd );
	}

}

static void FillCloudySkySide( const int mins[2], const int maxs[2], qboolean addIndexes ) {
	int s, t;
	int vertexStart = tess.numVertexes;
	int tHeight, sWidth;

	tHeight = maxs[1] - mins[1] + 1;
	sWidth = maxs[0] - mins[0] + 1;

	for ( t = mins[1] + HALF_SKY_SUBDIVISIONS; t <= maxs[1] + HALF_SKY_SUBDIVISIONS; t++ )
	{
		for ( s = mins[0] + HALF_SKY_SUBDIVISIONS; s <= maxs[0] + HALF_SKY_SUBDIVISIONS; s++ )
		{
			VectorAdd( s_skyPoints[t][s], backEnd.viewParms.or.origin, tess.xyz[tess.numVertexes] );
			tess.texCoords[tess.numVertexes][0][0] = s_skyTexCoords[t][s][0];
			tess.texCoords[tess.numVertexes][0][1] = s_skyTexCoords[t][s][1];

			tess.numVertexes++;

			if ( tess.numVertexes >= SHADER_MAX_VERTEXES ) {
				ri.Error( ERR_DROP, "SHADER_MAX_VERTEXES hit in FillCloudySkySide()\n" );
			}
		}
	}

	// only add indexes for one pass, otherwise it would draw multiple times for each pass
	if ( addIndexes ) {
		for ( t = 0; t < tHeight - 1; t++ )
		{
			for ( s = 0; s < sWidth - 1; s++ )
			{
				tess.indexes[tess.numIndexes] = vertexStart + s + t * ( sWidth );
				tess.numIndexes++;
				tess.indexes[tess.numIndexes] = vertexStart + s + ( t + 1 ) * ( sWidth );
				tess.numIndexes++;
				tess.indexes[tess.numIndexes] = vertexStart + s + 1 + t * ( sWidth );
				tess.numIndexes++;

				tess.indexes[tess.numIndexes] = vertexStart + s + ( t + 1 ) * ( sWidth );
				tess.numIndexes++;
				tess.indexes[tess.numIndexes] = vertexStart + s + 1 + ( t + 1 ) * ( sWidth );
				tess.numIndexes++;
				tess.indexes[tess.numIndexes] = vertexStart + s + 1 + t * ( sWidth );
				tess.numIndexes++;
			}
		}
	}
}

static void FillCloudBox( const shader_t *shader, int stage ) {
	int i;

	for ( i = 0; i < 6; i++ )
	{
		int sky_mins_subd[2], sky_maxs_subd[2];
		int s, t;
		float MIN_T;

		if ( 1 ) { // FIXME? shader->sky.fullClouds )
			MIN_T = -HALF_SKY_SUBDIVISIONS;

			// still don't want to draw the bottom, even if fullClouds
			if ( i == 5 ) {
				continue;
			}
		} else
		{
			switch ( i )
			{
			case 0:
			case 1:
			case 2:
			case 3:
				MIN_T = -1;
				break;
			case 5:
				// don't draw clouds beneath you
				continue;
			case 4:     // top
			default:
				MIN_T = -HALF_SKY_SUBDIVISIONS;
				break;
			}
		}

		sky_mins[0][i] = floor( sky_mins[0][i] * HALF_SKY_SUBDIVISIONS ) / HALF_SKY_SUBDIVISIONS;
		sky_mins[1][i] = floor( sky_mins[1][i] * HALF_SKY_SUBDIVISIONS ) / HALF_SKY_SUBDIVISIONS;
		sky_maxs[0][i] = ceil( sky_maxs[0][i] * HALF_SKY_SUBDIVISIONS ) / HALF_SKY_SUBDIVISIONS;
		sky_maxs[1][i] = ceil( sky_maxs[1][i] * HALF_SKY_SUBDIVISIONS ) / HALF_SKY_SUBDIVISIONS;

		if ( ( sky_mins[0][i] >= sky_maxs[0][i] ) ||
			 ( sky_mins[1][i] >= sky_maxs[1][i] ) ) {
			continue;
		}

		sky_mins_subd[0] = myftol( sky_mins[0][i] * HALF_SKY_SUBDIVISIONS );
		sky_mins_subd[1] = myftol( sky_mins[1][i] * HALF_SKY_SUBDIVISIONS );
		sky_maxs_subd[0] = myftol( sky_maxs[0][i] * HALF_SKY_SUBDIVISIONS );
		sky_maxs_subd[1] = myftol( sky_maxs[1][i] * HALF_SKY_SUBDIVISIONS );

		if ( sky_mins_subd[0] < -HALF_SKY_SUBDIVISIONS ) {
			sky_mins_subd[0] = -HALF_SKY_SUBDIVISIONS;
		} else if ( sky_mins_subd[0] > HALF_SKY_SUBDIVISIONS ) {
			sky_mins_subd[0] = HALF_SKY_SUBDIVISIONS;
		}
		if ( sky_mins_subd[1] < MIN_T ) {
			sky_mins_subd[1] = MIN_T;
		} else if ( sky_mins_subd[1] > HALF_SKY_SUBDIVISIONS ) {
			sky_mins_subd[1] = HALF_SKY_SUBDIVISIONS;
		}

		if ( sky_maxs_subd[0] < -HALF_SKY_SUBDIVISIONS ) {
			sky_maxs_subd[0] = -HALF_SKY_SUBDIVISIONS;
		} else if ( sky_maxs_subd[0] > HALF_SKY_SUBDIVISIONS ) {
			sky_maxs_subd[0] = HALF_SKY_SUBDIVISIONS;
		}
		if ( sky_maxs_subd[1] < MIN_T ) {
			sky_maxs_subd[1] = MIN_T;
		} else if ( sky_maxs_subd[1] > HALF_SKY_SUBDIVISIONS ) {
			sky_maxs_subd[1] = HALF_SKY_SUBDIVISIONS;
		}

		//
		// iterate through the subdivisions
		//
		for ( t = sky_mins_subd[1] + HALF_SKY_SUBDIVISIONS; t <= sky_maxs_subd[1] + HALF_SKY_SUBDIVISIONS; t++ )
		{
			for ( s = sky_mins_subd[0] + HALF_SKY_SUBDIVISIONS; s <= sky_maxs_subd[0] + HALF_SKY_SUBDIVISIONS; s++ )
			{
				MakeSkyVec( ( s - HALF_SKY_SUBDIVISIONS ) / ( float ) HALF_SKY_SUBDIVISIONS,
							( t - HALF_SKY_SUBDIVISIONS ) / ( float ) HALF_SKY_SUBDIVISIONS,
							i,
							NULL,
							s_skyPoints[t][s] );

				s_skyTexCoords[t][s][0] = s_cloudTexCoords[i][t][s][0];
				s_skyTexCoords[t][s][1] = s_cloudTexCoords[i][t][s][1];
			}
		}

		// only add indexes for first stage
		FillCloudySkySide( sky_mins_subd, sky_maxs_subd, ( stage == 0 ) );
	}
}

/*
** R_BuildCloudData
*/
void R_BuildCloudData( shaderCommands_t *input ) {
	int i;
	shader_t    *shader;

	shader = input->shader;

	assert( shader->isSky );

	sky_min = 1.0 / 256.0f;     // FIXME: not correct?
	sky_max = 255.0 / 256.0f;

	// set up for drawing
	tess.numIndexes = 0;
	tess.numVertexes = 0;

	if ( input->shader->sky.cloudHeight ) {
		for ( i = 0; i < MAX_SHADER_STAGES; i++ )
		{
			if ( !tess.xstages[i] ) {
				break;
			}
			FillCloudBox( input->shader, i );
		}
	}
}

/*
** R_InitSkyTexCoords
** Called when a sky shader is parsed
*/
#define SQR( a ) ( ( a ) * ( a ) )
void R_InitSkyTexCoords( float heightCloud ) {
	int i, s, t;
	float radiusWorld = 4096;
	float p;
	float sRad, tRad;
	vec3_t skyVec;
	vec3_t v;

	// init zfar so MakeSkyVec works even though
	// a world hasn't been bounded
	backEnd.viewParms.zFar = 1024;

	for ( i = 0; i < 6; i++ )
	{
		for ( t = 0; t <= SKY_SUBDIVISIONS; t++ )
		{
			for ( s = 0; s <= SKY_SUBDIVISIONS; s++ )
			{
				// compute vector from view origin to sky side integral point
				MakeSkyVec( ( s - HALF_SKY_SUBDIVISIONS ) / ( float ) HALF_SKY_SUBDIVISIONS,
							( t - HALF_SKY_SUBDIVISIONS ) / ( float ) HALF_SKY_SUBDIVISIONS,
							i,
							NULL,
							skyVec );

				// compute parametric value 'p' that intersects with cloud layer
				p = ( 1.0f / ( 2 * DotProduct( skyVec, skyVec ) ) ) *
					( -2 * skyVec[2] * radiusWorld +
					  2 * sqrt( SQR( skyVec[2] ) * SQR( radiusWorld ) +
								2 * SQR( skyVec[0] ) * radiusWorld * heightCloud +
								SQR( skyVec[0] ) * SQR( heightCloud ) +
								2 * SQR( skyVec[1] ) * radiusWorld * heightCloud +
								SQR( skyVec[1] ) * SQR( heightCloud ) +
								2 * SQR( skyVec[2] ) * radiusWorld * heightCloud +
								SQR( skyVec[2] ) * SQR( heightCloud ) ) );

				s_cloudTexP[i][t][s] = p;

				// compute intersection point based on p
				VectorScale( skyVec, p, v );
				v[2] += radiusWorld;

				// compute vector from world origin to intersection point 'v'
				VectorNormalize( v );

				sRad = Q_acos( v[0] );
				tRad = Q_acos( v[1] );

				s_cloudTexCoords[i][t][s][0] = sRad;
				s_cloudTexCoords[i][t][s][1] = tRad;
			}
		}
	}
}

//======================================================================================

/*
==============
RB_DrawSun
	(SA) FIXME: sun should render behind clouds, so passing dark areas cover it up
==============
*/
void RB_DrawSun( void ) {
	#if 0
	float size;
	float dist;
	vec3_t origin, vec1, vec2;
	vec3_t temp;
	byte color[4];

	if ( !tr.sunShader ) {
		return;
	}

	if ( !backEnd.skyRenderedThisView ) {
		return;
	}
	if ( !r_drawSun->integer ) {
		return;
	}
	qglLoadMatrixf( backEnd.viewParms.world.modelMatrix );
	qglTranslatef( backEnd.viewParms.or.origin[0], backEnd.viewParms.or.origin[1], backEnd.viewParms.or.origin[2] );

	dist =  backEnd.viewParms.zFar / 1.75;      // div sqrt(3)

	// (SA) shrunk the size of the sun
	size = dist * 0.2;

	VectorScale( tr.sunDirection, dist, origin );
	PerpendicularVector( vec1, tr.sunDirection );
	CrossProduct( tr.sunDirection, vec1, vec2 );

	VectorScale( vec1, size, vec1 );
	VectorScale( vec2, size, vec2 );

	// farthest depth range
	qglDepthRange( 1.0, 1.0 );
	RHI_CmdSetViewport( backEnd.viewParms.viewportX, backEnd.viewParms.viewportY,
		backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight, 1.0f, 1.0f );

	color[0] = color[1] = color[2] = color[3] = 255;

	// (SA) simpler sun drawing
	RB_BeginSurface( tr.sunShader, tess.fogNum );

	RB_AddQuadStamp( origin, vec1, vec2, color );

	RB_EndSurface();


	if ( r_drawSun->integer > 1 ) { // draw flare effect
		// (SA) FYI:	This is cheezy and was only a test so far.
		//				If we decide to use the flare business I will /definatly/ improve all this

		// get a point a little closer
		dist = dist * 0.7;
		VectorScale( tr.sunDirection, dist, origin );

		// and make the flare a little smaller
		VectorScale( vec1, 0.5f, vec1 );
		VectorScale( vec2, 0.5f, vec2 );

		// add the vectors to give an 'off angle' result
		VectorAdd( tr.sunDirection, backEnd.viewParms.or.axis[0], temp );
		VectorNormalize( temp );

		// amplify the result
		origin[0] += temp[0] * 500.0;
		origin[1] += temp[1] * 500.0;
		origin[2] += temp[2] * 500.0;

		// (SA) FIXME: todo: flare effect should render last (on top of everything else) and only when sun is in view (sun moving out of camera past degree n should start to cause flare dimming until view angle to sun is off by angle n + x.

		// draw the flare
		RB_BeginSurface( tr.sunflareShader[0], tess.fogNum );
		RB_AddQuadStamp( origin, vec1, vec2, color );
		RB_EndSurface();
	}

	// back to normal depth range
	qglDepthRange( 0.0, 1.0 );
	RHI_CmdSetViewport( backEnd.viewParms.viewportX, backEnd.viewParms.viewportY,
		backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight, 0.0f, 1.0f );
		#endif
}



extern void R_Fog( glfog_t *curfog );

/*
================
RB_StageIteratorSky

All of the visible sky triangles are in tess

Other things could be stuck in here, like birds in the sky, etc
================
*/
void RB_StageIteratorSky( void ) {
	if ( r_fastsky->integer ) {
		return;
	}

	// when portal sky exists, only render skybox for the portal sky scene
	if ( skyboxportal && !( backEnd.refdef.rdflags & RDF_SKYBOXPORTAL ) ) {
		return;
	}

	// does the current fog require fastsky?
	if ( backEnd.viewParms.glFog.registered ) {
		if ( !backEnd.viewParms.glFog.drawsky ) {
			return;
		}
	} else if ( glfogNum > FOG_NONE )      {
		if ( !glfogsettings[FOG_CURRENT].drawsky ) {
			return;
		}
	}


	backEnd.refdef.rdflags |= RDF_DRAWINGSKY;


	// go through all the polygons and project them onto
	// the sky box to see which blocks on each side need
	// to be drawn
	RB_ClipSkyPolygons( &tess );

	// r_showsky will let all the sky blocks be drawn in
	// front of everything to allow developers to see how
	// much sky is getting sucked in
	if ( r_showsky->integer ) {
		RHI_CmdSetViewport( backEnd.viewParms.viewportX, backEnd.viewParms.viewportY,
			backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight, 0.0f, 0.0f );
	} else {
		RHI_CmdSetViewport( backEnd.viewParms.viewportX, backEnd.viewParms.viewportY,
			backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight, 1.0f, 1.0f );
	}

	// draw the outer skybox
	if ( tess.shader->sky.outerbox[0] && tess.shader->sky.outerbox[0] != tr.defaultImage ) {
		//@TODO translate position for skybox backEnd.viewParms.or.origin
		DrawSkyBox( tess.shader );

	}

	// generate the vertexes for all the clouds, which will be drawn
	// by the generic shader routine
	R_BuildCloudData( &tess );

	RB_StageIteratorGeneric();

	// draw the inner skybox
	// Rafael - drawing inner skybox
	if ( tess.shader->sky.innerbox[0] && tess.shader->sky.innerbox[0] != tr.defaultImage ) {
		//@TODO translate position for skybox backEnd.viewParms.or.origin
		DrawSkyBoxInner( tess.shader );

	}
	// Rafael - end

	// back to normal depth range
	RHI_CmdSetViewport( backEnd.viewParms.viewportX, backEnd.viewParms.viewportY,
		backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight, 0.0f, 1.0f );

	backEnd.refdef.rdflags &= ~RDF_DRAWINGSKY;

	// note that sky was drawn so we will draw a sun later
	backEnd.skyRenderedThisView = qtrue;
}

