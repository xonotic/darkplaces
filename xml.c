#include "xml.h" 
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

typedef struct
{
	xmlDocPtr  doc;
	xmlNodePtr node;
	xmlAttrPtr attribute;
} qxml_t;

typedef qxml_t* qxml_p;

static qxml_p VM_Xml_Data(prvm_prog_t *prog, int index)
{
	if (index < 0 || index >= PRVM_MAX_OPENFILES)
	{
		Con_Printf("VM_Xml_Data: invalid file handle %i used in %s\n", index, prog->name);
		return NULL;
	}
	if (prog->open_xml_files[index] == NULL)
	{
		Con_Printf("VM_Xml_Data: no such file handle %i (or file has been closed) in %s\n", index, prog->name);
		return NULL;
	}
	return (qxml_p)prog->open_xml_files[index];
}

void XML_Close(prvm_prog_t *prog, int index)
{
	if ( prog->open_xml_files[index] != NULL )
	{
		qxml_p xml = (qxml_p)prog->open_xml_files[index];
		xmlFreeDoc(xml->doc);
		free(xml);
		prog->open_xml_files[index] = NULL;
	}
}

void VM_xml_open(prvm_prog_t *prog)
{
	const char* filename;
	
	char* data;
	size_t datasize;
	qfile_t* filepointer;
	char vabuf[1024];
	
	xmlDocPtr doc;
	int docid;
	qxml_p xml;
	
	VM_SAFEPARMCOUNT(1,VM_xmlopen);
	filename = PRVM_G_STRING(OFS_PARM0);
	
	
	for (docid = 0; docid < PRVM_MAX_OPENFILES; docid++)
		if (prog->open_xml_files[docid] == NULL)
			break;
	if (docid >= PRVM_MAX_OPENFILES)
	{
		PRVM_G_FLOAT(OFS_RETURN) = 0;
		VM_Warning(prog, "VM_xmlopen: %s ran out of file handles (%i)\n", prog->name, PRVM_MAX_OPENFILES);
		return;
	}
	
	filepointer = FS_OpenVirtualFile(va(vabuf, sizeof(vabuf), "data/%s", filename), false);
	if (filepointer == NULL)
		filepointer = FS_OpenVirtualFile(va(vabuf, sizeof(vabuf), "%s", filename), false);
	
	if ( filepointer == NULL )
	{
		PRVM_G_FLOAT(OFS_RETURN) = 0;
		VM_Warning(prog, "VM_xmlopen: Failed to open: %s\n", filename);
		return;
	}
	
	datasize = FS_FileSize(filepointer);
	data = malloc(datasize);
	FS_Read(filepointer,data,datasize);
	FS_Close(filepointer);

	doc = xmlParseMemory(data,datasize);
	free(data);
	
	if (doc == NULL) 
	{
		PRVM_G_FLOAT(OFS_RETURN) = 0;
		VM_Warning(prog, "VM_xmlopen: Failed to load XML: %s\n", filename);
		return;
	}
	
	xml = malloc(sizeof(qxml_t));
	xml->doc = doc;
	xml->node = xmlDocGetRootElement(doc);
	xml->attribute = NULL;
	prog->open_xml_files[docid] = xml;
	
	PRVM_G_FLOAT(OFS_RETURN) = docid+1; // ensure non-zero file id in QuakeC
}

void VM_xml_close(prvm_prog_t *prog)
{
	int fileindex;
	VM_SAFEPARMCOUNT(1,VM_xmlclose);
	fileindex = PRVM_G_FLOAT(OFS_PARM0);
	XML_Close(prog,fileindex-1);
}

#define VM_XML_CHECK_RETURN(funcname, errorreturn) \
	VM_SAFEPARMCOUNT(1,#funcname); \
	xml = VM_Xml_Data(prog,PRVM_G_FLOAT(OFS_PARM0)-1); \
	if ( !xml ) {\
		errorreturn; \
		return; \
	}\
	if ( !xml->node ) { \
		VM_Warning(prog, #funcname": null node in %s\n",prog->name); \
		errorreturn; \
		return; \
	}
#define VM_XML_CHECK(funcname) VM_XML_CHECK_RETURN(funcname,)

void VM_xml_tree_name(prvm_prog_t *prog)
{
	qxml_p xml;
	const xmlChar* xs;
	char s[VM_STRINGTEMP_LENGTH] = { 0 };
	VM_XML_CHECK(VM_xml_tree_name);
	if ( xml->attribute )
		xs = xml->attribute->name;
	else
		xs = xml->node->name;
	memcpy(s,xs,xmlStrlen(xs));
	PRVM_G_INT(OFS_RETURN) = PRVM_SetTempString(prog, s);
}

void VM_xml_tree_text(prvm_prog_t *prog)
{
	qxml_p xml;
	xmlChar* xs;
	char s[VM_STRINGTEMP_LENGTH] = { 0 };
	VM_XML_CHECK(VM_xml_tree_text);
	
	if ( xml->attribute )
	{
		xs = xmlGetProp(xml->node,xml->attribute->name);
		memcpy(s,xs,xmlStrlen(xs));
		xmlFree(xs);
	}
	else if ( xml->node->type == XML_ELEMENT_NODE )
	{
		xs = xmlNodeListGetString(xml->doc,xml->node->children,1);
		memcpy(s,xs,xmlStrlen(xs));
		xmlFree(xs);
	}
	else
		memcpy(s,xml->node->content,xmlStrlen(xml->node->content));
	PRVM_G_INT(OFS_RETURN) = PRVM_SetTempString(prog, s);
}

void VM_xml_tree_leaf(prvm_prog_t *prog)
{
	qxml_p xml;
	VM_XML_CHECK_RETURN(VM_xml_tree_leaf, PRVM_G_FLOAT(OFS_RETURN) = 0);
	if ( xml->attribute )
		PRVM_G_FLOAT(OFS_RETURN) = 1;
	else
		PRVM_G_FLOAT(OFS_RETURN) = !xml->node->children;
}

void VM_xml_tree_child(prvm_prog_t *prog)
{
	qxml_p xml;
	VM_XML_CHECK(VM_xml_tree_child);
	if ( xml->attribute )
		VM_Warning(prog, "VM_xml_tree_child: trying to get the child of attribute \"%s\"!\n",
				   xml->attribute->name);
	else if ( xml->node->children )
		xml->node = xml->node->children;
	else
		VM_Warning(prog, "VM_xml_tree_child: trying to get the child of leaf element \"%s\"!\n",
				   xml->node->name);	
}

void VM_xml_tree_parent(prvm_prog_t *prog)
{
	qxml_p xml;
	VM_XML_CHECK(VM_xml_tree_parent);
	if ( xml->attribute )
		xml->attribute = NULL;
	else if ( xml->node->parent )
		xml->node = xml->node->parent;
	else
		VM_Warning(prog, "VM_xml_tree_parent: trying to get the parent of root element \"%s\"!\n",
				   xml->node->name);
}

void VM_xml_tree_has_sibling(prvm_prog_t *prog)
{
	qxml_p xml;
	VM_XML_CHECK_RETURN(VM_xml_tree_has_sibling, PRVM_G_FLOAT(OFS_RETURN) = 0);
	if ( xml->attribute )
		PRVM_G_FLOAT(OFS_RETURN) = !!xml->attribute->next;
	else
		PRVM_G_FLOAT(OFS_RETURN) = !!xml->node->next;
}

void VM_xml_tree_next(prvm_prog_t *prog)
{
	qxml_p xml;
	VM_XML_CHECK(VM_xml_tree_next);
	if ( xml->attribute )
	{
		if ( xml->attribute->next )
			xml->attribute = xml->attribute->next;
		else
			VM_Warning(prog,"VM_xml_tree_next: trying to get next sibling of last attribute \"%s\"!\n",
				   xml->attribute->name);
	}
	else if ( xml->node->next )
		xml->node = xml->node->next;
	else
		VM_Warning(prog,"VM_xml_tree_next: trying to get next sibling of last element \"%s\"!\n",
				xml->node->name);
}

void VM_xml_tree_type(prvm_prog_t *prog)
{
	qxml_p xml;
	VM_XML_CHECK_RETURN(VM_xml_tree_type, PRVM_G_FLOAT(OFS_RETURN) = 0);
	if ( xml->attribute )
		PRVM_G_FLOAT(OFS_RETURN) = XML_ATTRIBUTE_NODE;
	else
		PRVM_G_FLOAT(OFS_RETURN) = xml->node->type;
}

void VM_xml_tree_root(prvm_prog_t *prog)
{
	qxml_p xml;
	VM_SAFEPARMCOUNT(1,VM_xml_tree_root);
	xml = VM_Xml_Data(prog,PRVM_G_FLOAT(OFS_PARM0)-1); 
	if ( !xml )
		return;
	xml->node = xmlDocGetRootElement(xml->doc);
}

void VM_xml_tree_attribute(prvm_prog_t *prog)
{
	qxml_p xml;
	VM_XML_CHECK_RETURN(VM_xml_tree_type, PRVM_G_FLOAT(OFS_RETURN) = 0);
	if ( !xml->attribute && xml->node->properties )
	{
		PRVM_G_FLOAT(OFS_RETURN) = 1;
		xml->attribute = xml->node->properties;
	}
	else
		PRVM_G_FLOAT(OFS_RETURN) = 0;
}

// =================================================
// Tiled Stuff
// =================================================

// Read an attribute as a float
static float xml_attribute_float( xmlNodePtr node, const char* attribute, float default_value )
{
	xmlChar* s;
	float r;
	
	r = default_value;
	if (( s = xmlGetProp(node, (xmlChar*)attribute) ))
	{
		sscanf((char*)s,"%f",&r);
		xmlFree(s);
	}
	return r;
}

// Read an attribute as an integer
static int xml_attribute_int( xmlNodePtr node, const char* attribute, int default_value )
{
	xmlChar* s;
	int r;
	
	r = default_value;
	if (( s = xmlGetProp(node, (xmlChar*)attribute) ))
	{
		sscanf((char*)s,"%d",&r);
		xmlFree(s);
	}
	return r;
}
// Read an attribute as a color (defaults to -1 -1 -1)
static void xml_attribute_color( xmlNodePtr node, const char* attribute, vec3_t output )
{
	xmlChar* s;
	int r, g, b;
	output[0] = output[1] = output[2] = -1;
	
	if (( s = xmlGetProp(node, (xmlChar*)attribute) ))
	{
		if ( sscanf((char*)s,"#%2x%2x%2x",&r,&g,&b) == 3 )
		{
			output[0] = r / 255.0;
			output[1] = g / 255.0;
			output[2] = b / 255.0;
		}
		xmlFree(s);
	}
}

// Read an attribute as a string
static char* xml_attribute_string( xmlNodePtr node, const char* attribute )
{
	xmlChar* s;
	char* r;
	int size;
	
	size = 0;
	r = 0;
	if (( s = xmlGetProp(node, (xmlChar*)attribute) ))
	{
		// xmlChar is just an unsigned char but copy just in case
		size = xmlStrlen(s);
		r = malloc(size);
		memcpy(r,s,size);
		xmlFree(s);
	}
	
	// ensure we always have a NUL-terminated string
	if ( !size )
	{
		free(r);
		r = malloc(1);
		r[0] = 0;
	}
	
	return r;
}

// x,y coordinate
typedef struct {
	long x;
	long y;
} Tiled_Coord;

// Single Property
typedef struct {
	char* name;
	char* value;
} Tiled_Property;

// Data structure to hold properties
// Currently singly-linked list, would be more efficient to have a red-black tree
typedef struct Tiled_PropertyNode Tiled_PropertyNode;
struct Tiled_PropertyNode {
	Tiled_Property property;
	Tiled_PropertyNode* next;
};

typedef struct
{
	Tiled_PropertyNode* first;
	Tiled_PropertyNode* last;
} Tiled_Properties;

// recursively clear the properties
static void tmx_properties_clean_node(Tiled_PropertyNode* prop)
{
	if ( !prop )
		return;
	if ( prop->next )
		tmx_properties_clean_node(prop->next);
	free(prop->property.name);
	free(prop->property.value);
	free(prop);
}

// clear the properties structure
static void tmx_properties_clean(Tiled_Properties props)
{
	tmx_properties_clean_node(props.first);
}

static void tmx_properties_insert(Tiled_Properties* out, xmlNodePtr xml)
{
	Tiled_PropertyNode* new_node;
	
	if ( strcmp((char*)xml->name,"properties") == 0  )
	{
		for ( xml = xml->xmlChildrenNode; xml; xml = xml->next )
			tmx_properties_insert(out,xml);
	}
	else if ( strcmp((char*)xml->name,"property") == 0  )
	{
		new_node = malloc(sizeof(Tiled_PropertyNode));
		new_node->property.name = xml_attribute_string(xml,"name");
		new_node->property.value = xml_attribute_string(xml,"value");
		new_node->next = NULL;
		if ( out->last )
		{
			out->last->next = new_node;
			out->last = new_node;
		}
		else
			out->first = new_node;
	}
}

// Image/Tile data
typedef struct {
	// Name of the image file
	char * source;
	// Width in pixels
	unsigned width;
	// Height in pixels
	unsigned height;
	// If used as a tile in a collection of images, relative ID
	unsigned id;
} Tiled_Image;

// Clean a Tiled_Image(Doesn't free the given pointer)
static void tmx_image_clean(Tiled_Image *img)
{
	free(img->source);
}

// Tileset
typedef struct {
	// Images of acollection or single image split in a grid
	Tiled_Image* image;
	// Number of elements in image
	unsigned image_count;
	// Name of the tileset
	char* name;
	// (Maximum) size of the tiles in the tileset
	Tiled_Coord tilesize;
	// Number of pixels between tiles
	unsigned spacing;
	// The margin around the tiles in this tileset 
	unsigned margin;
	// Offset to use when drawing
	Tiled_Coord tileoffset;
	
	Tiled_Properties properties;
} Tiled_Tileset;

// Common layer data
typedef struct Tiled_Layer Tiled_Layer;
struct Tiled_Layer 
{
	enum { TILED_TILELAYER, TILED_OBJECTLAYER, TILED_IMAGELAYER } type;
	char* name;
	// layer position in tiles
	Tiled_Coord pos;
	// layer size in tiles
	Tiled_Coord size;
	float opacity;
	qboolean visible;
	Tiled_Properties properties;
	// next layer in the map
	Tiled_Layer* next_layer;
};

// Tile layer
typedef struct {
	Tiled_Layer layer;
	unsigned long* data;
	unsigned data_size;
} Tiled_TileLayer;

// Object
typedef struct {
	char* name;
	char* type;
	// position in pixels
	Tiled_Coord pos;
	// size in pixels
	Tiled_Coord size;
	// In degress, clockwise
	double rotation;
	// Global tile ID
	unsigned gid;
	qboolean visible;
	Tiled_Properties properties;
} Tiled_Object;

// Object Group
typedef struct {
	Tiled_Layer layer;
	Tiled_Object * objects;
	unsigned objects_size;
} Tiled_ObjectLayer;

// Image layer
typedef struct {
	Tiled_Layer layer;
	Tiled_Image image;
} Tiled_ImageLayer;

// Load common data for <layer> <objectgroup> and <imagelayer>
static Tiled_Layer tmx_layer_common(xmlNodePtr xml_node)
{
	Tiled_Layer l;
	// TODO
	return l;
}

// Load <layer>
static Tiled_TileLayer* tmx_layer(xmlNodePtr xml_node)
{
	// TODO
	return NULL;
}

// load <imagelayer>
static Tiled_ImageLayer* tmx_imagelayer(xmlNodePtr xml_node)
{
	// TODO
	return NULL;
}

// load <objectgroup>
static Tiled_ObjectLayer* tmx_objectgroup(xmlNodePtr xml_node)
{
	// TODO
	return NULL;
}

// clean up <layer> and friends
static void tmx_layer_clean(Tiled_Layer* layer)
{
	if ( !layer )
		return;
	if ( layer->next_layer )
		tmx_layer_clean(layer->next_layer);
	free(layer->name);
	tmx_properties_clean(layer->properties);
	if ( layer->type == TILED_TILELAYER )
		free( ((Tiled_TileLayer*)layer)->data );
	else if ( layer->type == TILED_OBJECTLAYER )
		; // TODO clean objects
	else if ( layer->type == TILED_IMAGELAYER )
		tmx_image_clean(&((Tiled_ImageLayer*)layer)->image);
	
}

// Map
typedef struct {
	// File format version
	float version;
	enum { ORTHOGONAL, ISOMETRIC, STAGGERED } orientation;
	// size in tiles
	Tiled_Coord size;
	// size of a tile in pixels
	Tiled_Coord tilesize;
	// Color
	vec3_t backgroundcolor;
	// TODO renderorder
	Tiled_Properties properties;
	Tiled_Layer* first_layer;
} Tiled_Map;

// Load a TMX map from a data file
static Tiled_Map* tmx_map_load(const char* filename)
{	
	char* data;
	size_t datasize;
	qfile_t* filepointer;
	char vabuf[1024];
	
	Tiled_Map* map;
	Tiled_Layer* last_layer;
	Tiled_Layer* curr_layer;
	
	xmlDocPtr  doc;
	xmlNodePtr xml_map;
	xmlChar*   xml_tempstring;
	xmlNodePtr xml_tempnode;
	
	
	filepointer = FS_OpenVirtualFile(va(vabuf, sizeof(vabuf), "data/%s", filename), false);
	if (filepointer == NULL)
		filepointer = FS_OpenVirtualFile(va(vabuf, sizeof(vabuf), "%s", filename), false);
	
	if ( filepointer == NULL )
		return NULL;
	
	datasize = FS_FileSize(filepointer);
	data = malloc(datasize);
	FS_Read(filepointer,data,datasize);
	FS_Close(filepointer);

	doc = xmlParseMemory(data,datasize);
	free(data);
	
	if (doc == NULL) 
		return NULL;
	
	xml_map = xmlDocGetRootElement(doc);
	if ( !xml_map )
		return NULL;
	
	map = malloc(sizeof(Tiled_Map));
	
	map->version = xml_attribute_float(xml_map,"version",1);
	// TODO check version
	
	xml_tempstring = xmlGetProp(xml_map,(xmlChar*)"orientation");
	map->orientation = ORTHOGONAL;
	if ( xml_tempstring )
	{
		if ( strcmp((char*)xml_tempstring,"isometric") == 0 )
			map->orientation = ISOMETRIC;
		else if ( strcmp((char*)xml_tempstring,"staggered") == 0 )
			map->orientation = STAGGERED;
		xmlFree(xml_tempstring);
	}
	
	map->size.x = xml_attribute_int(xml_map,"width",0);
	map->size.y = xml_attribute_int(xml_map,"height",0);
	map->tilesize.x = xml_attribute_int(xml_map,"tilewidth",0);
	map->tilesize.y = xml_attribute_int(xml_map,"tileheight",0);
	if ( map->size.x <= 0 || map->size.y <= 0 || map->tilesize.x <= 0 || map->tilesize.y <= 0 )
	{
		free(map);
		xmlFreeDoc(doc);
		return NULL;
	}
	
	xml_attribute_color(xml_map,"backgroundcolor",map->backgroundcolor);
	
	map->first_layer = NULL;
	last_layer = NULL;
	map->properties.first = map->properties.last = NULL;
	for ( xml_tempnode = xml_map->xmlChildrenNode; xml_tempnode; xml_tempnode = xml_tempnode->next )
	{
		if ( xml_tempnode->type != XML_ELEMENT_NODE )
			continue;
		
		curr_layer = NULL;
		if ( strcmp((char*)xml_tempnode->name,"layer") == 0  )
			curr_layer = (Tiled_Layer*)tmx_layer(xml_tempnode);
		else if ( strcmp((char*)xml_tempnode->name,"objectgroup") == 0  )
			curr_layer = (Tiled_Layer*)tmx_objectgroup(xml_tempnode);
		else if ( strcmp((char*)xml_tempnode->name,"imagelayer") == 0  )
			curr_layer = (Tiled_Layer*)tmx_imagelayer(xml_tempnode);
		else if ( strcmp((char*)xml_tempnode->name,"properties") == 0  )
			tmx_properties_insert(&map->properties,xml_tempnode);
		// TODO: tilesets
		if ( curr_layer )
		{
			if ( !map->first_layer )
				map->first_layer = curr_layer;
			else
				last_layer->next_layer = curr_layer;
			last_layer = curr_layer;
		}
	}
	
	return map;
}

// Clean up the map struct
static void tmx_map_clean(Tiled_Map* map)
{
	tmx_properties_clean(map->properties);
	tmx_layer_clean(map->first_layer);
	free(map);
}

