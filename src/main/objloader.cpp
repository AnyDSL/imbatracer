#include "objloader.h"
//#include <rt/groups/group.h>
#include <core/assert.h>
#include <string>
#include <fstream>
//#include <rt/solids/triangle.h>
#include <set>
#include <cstring>

/*
#include <rt/materials/material.h>

#include <rt/materials/dummy.h>
#include <rt/coordmappers/local.h>

#include <rt/coordmappers/world.h>
#include <rt/coordmappers/tmapper.h>

#include <rt/solids/striangle.h>
*/

namespace rt {

namespace {
    using impala::Point;
    using impala::TexCoord;
    typedef impala::Vec Vector;

enum Instruction {
    Obj_None, //no instruction at all
    Obj_Invalid, //error while parsing an instruction
    Obj_Vertex, //geometric vertices
    Obj_TexVertex, //texture vertices
    Obj_NormalVertex, //vertex normals
    Obj_ParamVertex, //parameter space vertices - Free-form curve/surface attributes
    Obj_CurveVertex, //rational or non-rational forms of curve or surface type: basis matrix, Bezier, B-spline, Cardinal, Taylor
    Obj_Degree, //degree
    Obj_BasisMatrix, //basis matrix
    Obj_Step, //step size
    Obj_Point, //point
    Obj_Line, //line
    Obj_Face, //face
    Obj_Curve, //curve
    Obj_Curve2, //2D-curve
    Obj_Surface, //surface
    Obj_CurveParameter, //Free-form curve/surface body - parameter values
    Obj_CurveTrim, //Free-form curve/surface body - outer trimming loop
    Obj_CurveHole, //Free-form curve/surface body - inner trimming loop
    Obj_CurveSpecialCurve, //Free-form curve/surface body - special curve
    Obj_CurveSpecialPoint, //Free-form curve/surface body - special point
    Obj_CurveEnd, //Free-form curve/surface body - end
    Obj_Connect, //Connect free-form surfaces
    Obj_Group, //group
    Obj_Smooth, //smooth group
    Obj_MergingGroup, //merging group
    Obj_Object, //object name
    Obj_Bevel, //bevel interpolation
    Obj_ColorInterpolation, //color interpolation
    Obj_DissolveInterpolation, //dissolve interpolation
    Obj_LOD, //level of detail
    Obj_Material, //use material
    Obj_MaterialLibrary, //material library
    Obj_Shadow, //shadow casting
    Obj_Trace, //ray tracing
    Obj_ApproxCurve, //curve approximation technique
    Obj_ApproxSurface //surface approximation technique
};

struct Int3 {
    int vidx, tidx, nidx;
};
struct Float2 {
    float x, y;
    operator TexCoord() {
        return TexCoord(x,y);
    }
};
struct Float3 {
    float x, y, z;
    operator Point() {
        return Point(x,y,z);
    }
    operator Vector() {
        return Vector(x,y,z);
    }
};

struct FileLine {
    std::string str;
    size_t pos;
    size_t lineIdx;
    std::string filename;
    std::ifstream file;
    bool open(const std::string& filename);
    void close();
    void nextLine();
    bool eof() const { return file.eof(); }
    void removeComments();
    void skipWhitespace();
    bool match(const char* reference);
    Instruction fetchInstruction();
    float fetchFloat();
    float fetchFloat(float defaultv);
    std::string fetchString();
    Int3 fetchVertex();
};

bool FileLine::open( const std::string& infilename ) {
    filename = infilename;
    lineIdx = 0;
    pos = 0;
    file.open(filename.c_str(), std::ios_base::in);
    return !!file;
}

void FileLine::close() {
    file.close();
}

void FileLine::nextLine() {
    ++lineIdx;
    std::getline(file,str);
    pos = 0;
}

void FileLine::removeComments() {
    size_t idx=str.find('#');
    if (idx!=std::string::npos)
        str.erase(idx); //clear all elements starting from idx
}

void FileLine::skipWhitespace() {
    while (str.c_str()[pos]==' ' || str.c_str()[pos]=='\t' || str.c_str()[pos]=='\n' || str.c_str()[pos]=='\r')   ++pos;
}

bool FileLine::match(const char* reference) {
    size_t i=0;
    //check each character of "reference"
    while (reference[i]!=0) {
        if (str.c_str()[pos+i] != reference[i])
            return false;
        ++i;
    }
    //confirm that the input word ends right here
    char c=str.c_str()[pos+i];
    if (!((c>='A' && c<='Z') || (c>='a' && c<='z') || (c>='0' && c<='9')|| (c=='_'))) {
        pos = pos+i;
        return true;
    }
    return false;
}

Instruction FileLine::fetchInstruction() {
    // this seems to eb unused? Instruction i=Obj_None;
    skipWhitespace();

    if (match("")) return Obj_None;
    if (match("v")) return Obj_Vertex;
    if (match("vn")) return Obj_NormalVertex;
    if (match("vt")) return Obj_TexVertex;
    if (match("f")) return Obj_Face;
    if (match("usemtl")) return Obj_Material;
    if (match("mtllib")) return Obj_MaterialLibrary;
    if (match("o")) return Obj_Object;
    if (match("g")) return Obj_Group;
    if (match("p")) return Obj_Point;
    if (match("l")) return Obj_Line;
    if (match("mg")) return Obj_MergingGroup;
    if (match("vp")) return Obj_ParamVertex;
    if (match("surf")) return Obj_Surface;
    if (match("cstype")) return Obj_CurveVertex;
    if (match("curv")) return Obj_Curve;
    if (match("curv2")) return Obj_Curve2;
    if (match("con")) return Obj_Connect;
    if (match("c_interp")) return Obj_ColorInterpolation;
    if (match("ctech")) return Obj_ApproxCurve;
    if (match("deg")) return Obj_Degree;
    if (match("bmat")) return Obj_BasisMatrix;
    if (match("bevel")) return Obj_Bevel;
    if (match("sp")) return Obj_CurveSpecialPoint;
    if (match("step")) return Obj_Step;
    if (match("scrv")) return Obj_CurveSpecialCurve;
    if (match("shadow_obj")) return Obj_Shadow;
    if (match("stech")) return Obj_ApproxSurface;
    if (match("s")) return Obj_Smooth;
    if (match("parm")) return Obj_CurveParameter;
    if (match("lod")) return Obj_LOD;
    if (match("trim")) return Obj_CurveTrim;
    if (match("trace_obj")) return Obj_Trace;
    if (match("hole")) return Obj_CurveHole;
    if (match("end")) return Obj_CurveEnd;
    if (match("d_interp")) return Obj_DissolveInterpolation;
    return Obj_Invalid;
};

float FileLine::fetchFloat() {
    skipWhitespace();
    const char* src=str.c_str()+pos;
    char *dest;
    float f=float(strtod(src,&dest));
    release_assert(dest!=src, "Error in file ", filename, ":", lineIdx, ".", pos, " : expected a floating-point number");
    pos+=(dest-src);
    return f;
}

float FileLine::fetchFloat(float defaultv) {
    skipWhitespace();
    const char* src=str.c_str()+pos;
    char *dest;
    float f=float(strtod(src,&dest));
    if (dest==src)
        f=defaultv;
    else
        pos+=dest-src;
    return f;
}

std::string FileLine::fetchString() {
    skipWhitespace();
    size_t start = pos;
    while (str.c_str()[pos]!=' ' && str.c_str()[pos]!='\t' && str.c_str()[pos]!='\n' && str.c_str()[pos]!='\r' && str.c_str()[pos]!=0)   ++pos;
    return str.substr(start, pos-start);
}

Int3 FileLine::fetchVertex() {
    skipWhitespace();
    const char* src=str.c_str()+pos;
    char *dest;
    Int3 i;
    i.vidx = i.tidx = i.nidx = 0;
    i.vidx=strtol(src,&dest,10);
    if (dest==src) return i; // (0,0,0)
    pos+=dest-src;
    skipWhitespace();
    if (str.c_str()[pos]!='/') return i;
    ++pos;
    skipWhitespace();
    if (str.c_str()[pos]!='/') {
        src = str.c_str()+pos;
        i.tidx=strtol(src,&dest,10);
        release_assert(dest!=src, "Error in file ", filename, ":", lineIdx, ".", pos, " : Expected an integer (texture coordinate index)");
        pos+=dest-src;
        skipWhitespace();
        if (str.c_str()[pos]!='/') return i;
    }
    ++pos;
    skipWhitespace();
    src = str.c_str()+pos;
    i.nidx=strtol(src,&dest,10);
    release_assert(dest!=src, "Error in file ", filename, ":", lineIdx, ".", pos, " : Expected an integer (vertex normal index)");
    pos+=dest-src;
    return i;
}
}


namespace {

    struct MaterialInfo {
        std::string name;
        impala::Material mat;

        MaterialInfo() : mat(impala::Material::dummy()) {}
    };

    void skipWS(const char * &aStr)
    {
        while(isspace(*aStr))
            aStr++;
    }

    std::string endSpaceTrimmed(const char* _str)
    {
        size_t len = std::strlen(_str);
        const char *firstChar = _str;
        const char *lastChar = firstChar + len - 1;
        while(lastChar >= firstChar && isspace(*lastChar))
            lastChar--;

        return std::string(firstChar, lastChar + 1);
    }

    void matCreate(MatLib* dest, MaterialInfo& matinfo, impala::Material *mats, size_t nmats, size_t *nmatsOverridden) {
        if(mats && *nmatsOverridden < nmats)
            matinfo.mat = mats[*nmatsOverridden++];
		if (matinfo.name.length() && dest->find(matinfo.name) == dest->end()) { // do not overwrite stuff
            std::cerr << "creating material " << matinfo.name << "\n";
            dest->insert(make_pair(matinfo.name, matinfo.mat));
        }
        matinfo = MaterialInfo();
    }
}


void loadOBJMat(Scene *scene, MatLib* dest, const std::string& path, const std::string& filename, impala::Material *mats, size_t nmats, size_t *nmatsOverridden) {
    std::string fullname = path + filename;
    std::ifstream matInput(fullname.c_str(), std::ios_base::in);
    std::string buf;
    MaterialInfo material;


    if(matInput.fail()) {
        std::cerr << "Failed to open file " << fullname << "\n";
        return;
    }

    size_t curLine = 0;

    while(!matInput.eof())
    {
        std::getline(matInput, buf);
        curLine++;
        const char* cmd = buf.c_str();
        skipWS(cmd);

        if(std::strncmp(cmd, "newmtl", 6) == 0)
        {
            matCreate(dest, material, mats, nmats, nmatsOverridden); //create the previous material (if it exists) and clear the material info
            cmd += 6;

            skipWS(cmd);
            material.name = endSpaceTrimmed(cmd);
        }
        else if(
            std::strncmp(cmd, "Kd", 2) == 0 || std::strncmp(cmd, "Ks", 2) == 0
            || std::strncmp(cmd, "Ka", 2) == 0)
        {
            char coeffType = *(cmd + 1);

            impala::Color color;
            cmd += 2;

            char *newCmdString;
            for(int i = 0; i < 3; i++)
            {
                skipWS(cmd);
                color[i] = (float)strtod(cmd, &newCmdString);
                if(newCmdString == cmd) goto parse_err_found;
                cmd = newCmdString;
            }

            // TODO: use delayed texture allocation in case a material isn't used
            switch (coeffType)
            {
            case 'd':
                material.mat.diffuse = scene->addTexture(impala::Texture::constant(color)); break;
            case 'a':
                material.mat.emissive = scene->addTexture(impala::Texture::constant(color)); break;
            case 's':
                if (material.mat.specExp < 0) material.mat.specExp = 1;
                material.mat.specular = scene->addTexture(impala::Texture::constant(color)); break;
            }
        }
        else if(std::strncmp(cmd,  "Ns", 2) == 0)
        {
            cmd += 2;

            char *newCmdString;
            skipWS(cmd);
            float coeff = (float)strtod(cmd, &newCmdString);
            if(newCmdString == cmd) goto parse_err_found;
            cmd = newCmdString;
            material.mat.specExp = coeff;
        }
        else if(
            std::strncmp(cmd, "map_Kd", 6) == 0 || std::strncmp(cmd, "map_Ks", 6) == 0
            || std::strncmp(cmd, "map_Ka", 6) == 0) {
                std::cerr << "Image textures not supported in " << fullname << std::endl;
        }

        continue;
parse_err_found:
        std::cerr << "Error at line " << curLine << "in " << fullname <<std::endl;
    }
    matCreate(dest, material, mats, nmats, nmatsOverridden);
}


FileObject::FileObject(const std::string &path, const std::string &filename, Scene *scene, impala::Material *mats, size_t nmats, unsigned flags)
{
    MatLib* matlib = new MatLib;

    std::map<std::string, unsigned> materialName2Idx;
    std::set<Instruction> unsupportedEncounters;
    std::set<std::string> unknownMaterialEncounters;

    unsigned curMatIdx = 0; // the default material added in scene ctor

    size_t nmatsOverridden = 0;

    FileLine fileline;
    if(!fileline.open(path+filename)) {
        std::cerr << "ERROR: Cannot open " << filename << std::endl;
        return;
    }

    while (!fileline.eof()) {
        fileline.nextLine();
        fileline.removeComments();
        Instruction i = fileline.fetchInstruction();
        switch (i) {
            case Obj_Vertex:
            case Obj_NormalVertex: {
                Float3 v;
                v.x = fileline.fetchFloat();
                v.y = fileline.fetchFloat();
                v.z = fileline.fetchFloat();
                float w = fileline.fetchFloat(1.0f);
                if (w != 1.0f) { v.x/=w; v.y/=w; v.z/=w; }
                if (i == Obj_Vertex)
                    verts.push_back(v);
                else
                    normals.push_back(Vector(v).normal());
                break;
            }
            case Obj_TexVertex: {
                Float2 v;
                v.x = fileline.fetchFloat();
                v.y = 1-fileline.fetchFloat(0.0f);
                fileline.fetchFloat(0.0f); //u, v, w. Ignore w
                texCoords.push_back(v);
                break;
            }
            case Obj_Face: {
                bool skiptex = (flags & IgnoreTexCoord);
                bool skipnormal = (flags & IgnoreNormals);

                Int3 v[3];

                v[0] = fileline.fetchVertex();
                release_assert(v[0].vidx != 0, "Error in file ", fileline.filename, ":", fileline.lineIdx, ".", fileline.pos, " : Vertex index cannot be 0");
                if (v[0].vidx<0) v[0].vidx = verts.size() - v[0].vidx; else { --v[0].vidx; }
                if (v[0].tidx == 0) skiptex=true;
                else if (v[0].tidx<0) v[0].tidx = texCoords.size() - v[0].tidx; else { --v[0].tidx; }
                if (v[0].nidx == 0) skipnormal=true;
                else if (v[0].nidx<0) v[0].nidx = normals.size() - v[0].nidx; else { --v[0].nidx; }


                v[1] = fileline.fetchVertex();
                release_assert(v[1].vidx != 0, "Error in file ", fileline.filename, ":", fileline.lineIdx, ".", fileline.pos, " : Vertex index cannot be 0");
                if (v[1].vidx<0) v[1].vidx = verts.size() - v[1].vidx; else { --v[1].vidx; }
                if (v[1].tidx == 0) skiptex=true;
                else if (v[1].tidx<0) v[1].tidx = texCoords.size() - v[1].tidx; else { --v[1].tidx; }
                if (v[1].nidx == 0) skipnormal=true;
                else if (v[1].nidx<0) v[1].nidx = normals.size() - v[1].nidx; else { --v[1].nidx; }

                v[2] = fileline.fetchVertex();
                release_assert(v[2].vidx != 0, "Error in file ", fileline.filename, ":", fileline.lineIdx, ".", fileline.pos, " : Vertex index cannot be 0");
                if (v[2].vidx<0) v[2].vidx = verts.size() - v[2].vidx; else { --v[2].vidx; }
                if (v[2].tidx == 0) skiptex=true;
                else if (v[2].tidx<0) v[2].tidx = texCoords.size() - v[2].tidx; else { --v[2].tidx; }
                if (v[2].nidx == 0) skipnormal=true;
                else if (v[2].nidx<0) v[2].nidx = normals.size() - v[2].nidx; else { --v[2].nidx; }

                while(true) {
                    tris.push_back(Tri(v[0].vidx, v[1].vidx, v[2].vidx,
                                       skipnormal ? NoIdx : v[0].nidx, skipnormal ? NoIdx : v[1].nidx, skipnormal ? NoIdx : v[2].nidx,
                                       skiptex ? NoIdx : v[0].tidx, skiptex ? NoIdx : v[1].tidx, skiptex ? NoIdx : v[2].tidx,
                                       curMatIdx));

                    // advance to next vertex
                    v[1] = v[2];
                    v[2] = fileline.fetchVertex();
                    if (v[2].vidx == 0) break;
                    if (v[2].vidx<0) v[2].vidx = verts.size() - v[2].vidx; else { --v[2].vidx; }
                    if (v[2].tidx == 0) skiptex=true;
                    else if (v[2].tidx<0) v[2].tidx = texCoords.size() - v[2].tidx; else { --v[2].tidx; }
                    if (v[2].nidx == 0) skipnormal=true;
                    else if (v[2].nidx<0) v[2].nidx = normals.size() - v[2].nidx; else { --v[2].nidx; }
                }
                break;
            }
            case Obj_MaterialLibrary: {
                if (!(flags & IgnoreMatLibs)) {
                    std::string libname = fileline.fetchString();
                    loadOBJMat(scene, matlib, path, libname, mats, nmats, &nmatsOverridden);
                }
                break;
            }
            case Obj_Material: {
                std::string matname = fileline.fetchString();
                auto i = matlib->find(matname);
                if (i != matlib->end()) {
                    // check if we already registered this material
                    auto j = materialName2Idx.find(matname);
                    if (j != materialName2Idx.end()) {
                        curMatIdx = j->second; // we have the surface index here
                    }
                    else {
                        // register material
                        curMatIdx = scene->addMaterial(i->second);
                        materialName2Idx.insert(std::make_pair(matname, curMatIdx));
                    }
                }
                else {
                    if (unknownMaterialEncounters.find(matname) == unknownMaterialEncounters.end()) {
                        std::cerr << "Warning: Material \'" << matname << "\' not found in material library at " << fileline.filename << ":" << fileline.lineIdx << "." << fileline.pos << ". Using dummy material." << std::endl;
                        unknownMaterialEncounters.insert(matname);
                    }
                    curMatIdx = NoIdx;
                }
                break;
            }
            case Obj_None: break; //empty line
            case Obj_Invalid:
                release_assert(false, "Error in file ", fileline.filename, ":", fileline.lineIdx, ".", fileline.pos, " : Vertex index cannot be 0");
            default:
                if (unsupportedEncounters.insert(i).second) //element inserted (was not there previously)
                    std::cerr << "Warning: Unsupported OBJ instruction encountered at " << fileline.filename << ":" << fileline.lineIdx << "." << fileline.pos << ", ignoring.\n";
        }
    }
    fileline.close();
    delete matlib;

    std::cout << "ObjLoader: Loaded " << verts.size() << " verts, " << normals.size() << " normals, " << texCoords.size() << " texcoords" << std::endl;
}

/*
void ObjLoader::updateMaterials(MatLib *matlib)
{
    // for all out surfaces, maybe we have to update them
    for (std::vector<CountedPtr<MeshSurface> >::iterator it = surfaces.begin(); it != surfaces.end(); ++it) {
        MatLib::iterator matit = matlib->find((*it)->name);
        if (matit != matlib->end()) {
            (*it)->material = matit->second;
        }
    }
}
*/

}

