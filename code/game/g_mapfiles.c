/*
===========================================================================
Copyright (C) 2010-2011 Manuel Wiese

This file is part of AfterShock source code.

AfterShock source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

AfterShock source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with AfterShock source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include "g_local.h"

#define MAX_MAPFILE_LENGTH 20000

#define MAX_TOKENNUM 2048

typedef enum {
	TOT_LPAREN,
	TOT_RPAREN,
	TOT_WORD,
	TOT_NUMBER,
	TOT_NIL,
	TOT_MAX
} tokenType_t;

#define TOKENVALUE_SIZE 64

typedef struct {
	char value[TOKENVALUE_SIZE];
	int type;
} token_t;

token_t tokens2[MAX_TOKENNUM];

//TODO: items at first, later more stuff

qboolean G_ClassnameAllowed( char *input ){
	gitem_t	*it;
	for ( it = bg_itemlist + 1 ; it->classname ; it++) {
		if ( !strcmp(input, it->classname ) ) {
			return qtrue;
		}
	}
	return qfalse;
}

void G_ClearEntities( void ){
	int i;
	for( i = 0; i < MAX_GENTITIES; i++ ) {
		if( !g_entities[i].inuse )
			continue;
		if( !G_ClassnameAllowed(g_entities[i].classname) )
			continue;
		g_entities[i].nextthink = 0;
		trap_UnlinkEntity(&g_entities[i]);
		
	}
}

void G_AddEntity( char* classname, vec3_t origin, int spawnflags, float wait, float random ){
	gentity_t *ent;
	ent = G_Spawn();
	ent->classname = classname;
	ent->spawnflags = spawnflags;
	ent->random = random;
	ent->wait = wait;
	
	VectorCopy( origin, ent->s.origin );
	VectorCopy( ent->s.origin, ent->s.pos.trBase );
	VectorCopy( ent->s.origin, ent->r.currentOrigin );
	
	// if we didn't get a classname, don't bother spawning anything
	if ( !G_CallSpawn( ent ) ) {
		G_FreeEntity( ent );
	}
}

static int G_setTokens2( char* in, char* out, int start ){
	int i = 0;
	while ( in[ start + i ] != ' ' ){
		if( in[ start + i ] == '\0' ){
			out[i] = in[start+1];
			return MAX_MAPFILE_LENGTH;
		}
		out[i] = in[start+i];
		i++;
	}
	out[i] = '\0';
	return start+i+1;
}

static int G_setTokenType2( char* value ){
	int count = 0;
	qboolean lpar= qfalse,rpar= qfalse,number= qfalse, character = qfalse;
	
	while( value[count] != '\0' ){
		if( value[count] == '{' )
			lpar = qtrue;
		else if( value[count] == '}' )
			rpar = qtrue;
		else if( value[count] >= '0' && value[count] <= '9' )
			number = qtrue;
		else if( ( value[count] >= 'a' && value[count] <= 'z' ) || ( value[count] >= 'A' && value[count] <= 'Z' ) )
			character = qtrue;
		count++;
	}
	
	if( lpar && !( rpar || number || character ) )
		return TOT_LPAREN;
	else if( rpar && !( lpar || number || character ) )
		return TOT_RPAREN;
	else if( number && !( lpar || rpar || character ) )
		return TOT_NUMBER;
	else if( character && !( lpar || rpar ) )
		return TOT_WORD;
	else
		return TOT_NIL;
}

static int G_FindNextToken2( char *find, token_t *in, int start ){
	int i;
	int cmp;
	
	for( i = start; i < MAX_TOKENNUM; i++ ){
		cmp= strcmp( in[i].value, find );
		if( cmp == 0 )
			return i;
	}
	return -1;
}

static qboolean G_AbeforeB2( char *A, char *B, token_t *in, int start ){
	int a = G_FindNextToken2( A, in, start );
	int b = G_FindNextToken2( B, in, start );
	
	if( b == -1 && a != -1 )
		return qtrue;
	if( a == -1 && b != -1 )
		return qfalse;
	if( a < b )
		return qtrue;
	else
		return qfalse;
}

//
// fields are needed for spawning from the entity string
//
typedef enum {
	F_INT, 
	F_FLOAT,
	F_LSTRING,			// string on disk, pointer in memory, TAG_LEVEL
	F_GSTRING,			// string on disk, pointer in memory, TAG_GAME
	F_VECTOR,
	F_ANGLEHACK,
	F_ENTITY,			// index on disk, pointer in memory
	F_ITEM,				// index on disk, pointer in memory
	F_CLIENT,			// index on disk, pointer in memory
	F_IGNORE
} fieldtypeCopy_t;

typedef struct
{
	char	*name;
	int		ofs;
	fieldtypeCopy_t	type;
	int		flags;
} fieldCopy_t;

fieldCopy_t fieldsCopy[] = {
	{"classname", FOFS(classname), F_LSTRING},
	{"origin", FOFS(s.origin), F_VECTOR},
	{"model", FOFS(model), F_LSTRING},
	{"model2", FOFS(model2), F_LSTRING},
	{"spawnflags", FOFS(spawnflags), F_INT},
	{"speed", FOFS(speed), F_FLOAT},
	{"target", FOFS(target), F_LSTRING},
	{"targetname", FOFS(targetname), F_LSTRING},
	{"message", FOFS(message), F_LSTRING},
	{"team", FOFS(team), F_LSTRING},
	{"wait", FOFS(wait), F_FLOAT},
	{"random", FOFS(random), F_FLOAT},
	{"count", FOFS(count), F_INT},
	{"health", FOFS(health), F_INT},
	{"light", 0, F_IGNORE},
	{"dmg", FOFS(damage), F_INT},
	{"angles", FOFS(s.angles), F_VECTOR},
	{"angle", FOFS(s.angles), F_ANGLEHACK},
	{"targetShaderName", FOFS(targetShaderName), F_LSTRING},
	{"targetShaderNewName", FOFS(targetShaderNewName), F_LSTRING},

	{NULL}
};

static void G_LoadMapfileEntity( token_t *in, int min, int max ){
	int i = min;
	int j;
	char buf[64];
	char *classname;
	vec3_t origin;
	int spawnflags;
	float random;
	float wait;
	qboolean foundClass=qfalse, foundOrigin=qfalse;
	for( i = min; i <= max; i++ ){
		//G_Printf("%s\n", in[i].value );
		if( !strcmp(in[i].value, "\"classname\"" ) ) {
			classname = va("%s", in[i+1].value );
			for( j = 0; j < strlen(classname) - 2; j++ ){
				classname[j] = classname[j+1];
			}
			classname[j] = '\0';
			//G_Printf("%s %s\n", in[i].value, in[i+1].value );
			foundClass = qtrue;
			
		} else if( !strcmp(in[i].value, "\"origin\"" ) ) {
		  
		//	buf = va("%s", in[i+1].value);
			Com_sprintf(buf, sizeof(buf), "%s", in[i+1].value);
			for( j = 0; j < strlen(buf) - 2; j++ ){
				buf[j] = buf[j+1];
			}
			buf[j+1] = '\0';
			
			origin[0] = atof(buf);
			origin[1] = atof(in[i+2].value);
			
			//buf = va("%s", in[i+3].value);
			Com_sprintf(buf, sizeof(buf), "%s", in[i+3].value);
			buf[strlen(buf)-1] = '\0';
			
			origin[2] = atof(buf);
			
			//G_Printf("%f %f %f\n", origin[0], origin[1], origin[2] );
			foundOrigin = qtrue;
		} else if( !strcmp(in[i].value, "\"spawnflags\"" ) ) {
		  
			//buf = va("%s", in[i+1].value);
			Com_sprintf(buf, sizeof(buf), "%s", in[i+1].value);
			for( j = 0; j < strlen(buf) - 2; j++ ){
				buf[j] = buf[j+1];
			}
			buf[j] = '\0';
			
			spawnflags = atoi(buf);
		} else if( !strcmp(in[i].value, "\"random\"" ) ) {
		  
			//buf = va("%s", in[i+1].value);
			Com_sprintf(buf, sizeof(buf), "%s", in[i+1].value);
			for( j = 0; j < strlen(buf) - 2; j++ ){
				buf[j] = buf[j+1];
			}
			buf[j] = '\0';
			
			random = atof(buf);
		} else if( !strcmp(in[i].value, "\"wait\"" ) ) {
		  
			//buf = va("%s", in[i+1].value);
			Com_sprintf(buf, sizeof(buf), "%s", in[i+1].value);
			for( j = 0; j < strlen(buf) - 2; j++ ){
				buf[j] = buf[j+1];
			}
			buf[j] = '\0';
			
			wait = atof(buf);
		}
	}
	if( foundClass && foundOrigin ){
		//G_Printf("%s\n", classname );
		G_AddEntity(classname, origin, spawnflags, wait, random);
	}
}

void G_LoadMapfile( char *filename ){
	char buffer[MAX_MAPFILE_LENGTH];
	qboolean lastSpace = qtrue;
	qboolean pgbreak = qfalse;
	int i = 0;
	int charCount = 0;
	int tokenNum = 0;
	int maxTokennum;
	int lpar, rpar;
	int len;
	fileHandle_t	f;
	
	len = trap_FS_FOpenFile ( filename, &f, FS_READ );
	
	if ( !f ) {
		G_Printf( "%s",va( S_COLOR_YELLOW "mapfile not found: %s\n", filename ) );	
		return;
	}

	if ( len >= MAX_MAPFILE_LENGTH ) {
		trap_Error( va( S_COLOR_RED "menu file too large: %s is %i, max allowed is %i", filename, len, MAX_MAPFILE_LENGTH ) );
		trap_FS_FCloseFile( f );
		return;
	}
	ClearRegisteredItems();
	G_ClearEntities();

	trap_FS_Read( buffer, len, f );
	buffer[len] = 0;
	trap_FS_FCloseFile( f );
	
	COM_Compress(buffer);
	
	for ( i = 0; i < MAX_MAPFILE_LENGTH; i++ ){
		
		//Filter comments( start at # and end at break )
		if( buffer[i] == '#' ){
			while( i < MAX_MAPFILE_LENGTH && !pgbreak ){
				if( buffer[i] == '\n' || buffer[i] == '\r' )
					pgbreak = qtrue;
				i++;
			}
			pgbreak = qfalse;
			lastSpace = qtrue;
			//continue;
		}
		
		if( SkippedChar( buffer[i] ) ){
			if( !lastSpace ){
				buffer[charCount] = ' ';
				charCount++;
				lastSpace = qtrue;
			}
			continue;
		}
		
		lastSpace = qfalse;
		buffer[charCount] = buffer[i];
		charCount++;
	}
	
	i = 0;
	while( i < MAX_MAPFILE_LENGTH && tokenNum < MAX_TOKENNUM){
		i = G_setTokens2( buffer, tokens2[tokenNum].value, i);
		tokens2[tokenNum].type = G_setTokenType2( tokens2[tokenNum].value );
		tokenNum++;
	}
	maxTokennum = tokenNum;
	
	G_Printf("Mapfile parser found %i tokens\n", maxTokennum );
	
	for( tokenNum = 0; tokenNum < maxTokennum; tokenNum++ ){
			if( strcmp( tokens2[tokenNum].value, "{" ) == 0 ){
				//CG_Printf("lpar found\n");
				lpar = tokenNum;
				if( G_AbeforeB2((char*)"{",(char*)"}", tokens2, tokenNum+2)){
					G_Printf("error: \"}\" expected at %s\n", tokens2[tokenNum].value);
					break;
				}
				//CG_Printf("debug abeforeb\n");
				rpar = G_FindNextToken2((char*)"}", tokens2, tokenNum+2 );
				//CG_Printf("debug findnexttoken\n");
				if( rpar != -1 ){
					G_LoadMapfileEntity(tokens2, lpar+1, rpar-1);
					//G_setHudElement(i, tokens, lpar+1, rpar-1);
					tokenNum = rpar;
				}	
			}	
	}
	SaveRegisteredItems();
	
}

void G_LoadMapfile_f( void ) {
	char buf[MAX_QPATH];
	char mapname[64];
	
	if ( trap_Argc() < 2 ) {
                G_Printf("Usage:  loadmapfile <filename>\n");
		return;
	}
	
	trap_Argv( 1, buf, sizeof( buf ) );
	
	G_LoadMapfile(buf);
	trap_Cvar_Set("mapfile",buf);
	trap_Cvar_VariableStringBuffer("mapname", mapname, sizeof(mapname));
	trap_Cvar_Set("lastmap",mapname);
}

void G_WriteMapfile_f( void ) {
	int i;
	fileHandle_t f;
	char *string;
	char buf[MAX_QPATH];
	fieldCopy_t *field;
	byte	*b;
	
	if ( trap_Argc() < 2 ) {
                G_Printf("Usage:  writemapfile <filename>\n");
		return;
	}
	
	trap_Argv( 1, buf, sizeof( buf ) );
	
	trap_FS_FOpenFile(va("%s", buf ),&f,FS_WRITE);
	
	for( i = 0; i < MAX_GENTITIES; i++ ){
	  
		if( !g_entities[i].inuse )
			continue;
		
		if( !G_ClassnameAllowed(g_entities[i].classname) )
			continue;
		b = (byte *) &g_entities[i];
		
		string = va("{\n");
		trap_FS_Write(string, strlen(string), f);
		
		for( field=fieldsCopy; field->name; field++ ){
			switch( field->type ) {
			case F_LSTRING:
				if( *(char **)(b+field->ofs) ){
					string = va("   \"%s\"   \"%s\"\n", field->name, *(char **)(b+field->ofs) );
					trap_FS_Write(string, strlen(string), f);
				}
				break;
			case F_VECTOR:
				if( ((float *)(b+field->ofs))[0] && ((float *)(b+field->ofs))[1] && ((float *)(b+field->ofs))[2] || !strcmp(field->name,"origin") ){
					string = va("   \"%s\"   \"%f %f %f\"\n", field->name, ((float *)(b+field->ofs))[0], ((float *)(b+field->ofs))[1], ((float *)(b+field->ofs))[2] );
					trap_FS_Write(string, strlen(string), f);
				}
				break;
			case F_INT:
				if( *(int *)(b+field->ofs) ){
					string = va("   \"%s\"   \"%i\"\n", field->name, *(int *)(b+field->ofs) );
					trap_FS_Write(string, strlen(string), f);
				}
				break;
			case F_FLOAT:
				if( *(float *)(b+field->ofs) ){
					string = va("   \"%s\"   \"%f\"\n", field->name, *(float *)(b+field->ofs) );
					trap_FS_Write(string, strlen(string), f);
				}
				break;
			case F_ANGLEHACK:
				if( ((float *)(b+field->ofs))[0] && ((float *)(b+field->ofs))[1] && ((float *)(b+field->ofs))[2] ){
					string = va("   \"%s\"   \"%f %f %f\"\n", field->name, ((float *)(b+field->ofs))[0], ((float *)(b+field->ofs))[1], ((float *)(b+field->ofs))[2] );
					trap_FS_Write(string, strlen(string), f);
				}
				break;
			default:
			case F_IGNORE:
				break;
			}
		}
		string = va("}\n\n");
		trap_FS_Write(string, strlen(string), f);
	}
	trap_FS_FCloseFile(f);
}