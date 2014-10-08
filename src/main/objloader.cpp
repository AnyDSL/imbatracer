#include "objloader.h"
#include <core/assert.h>
#include <string>
#include <fstream>
#include <set>
#include <cstring>


namespace rt {

namespace {

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
        unsigned matidx; // if this is NOT noidx, ignore the material component and use this index instead

        MaterialInfo() { impala_dummyMaterial(&mat); matidx = impala::impala_noIdx(); }
    };
    typedef std::map<std::string, MaterialInfo > MatLib;

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

    void matCreate(MatLib* dest, MaterialInfo& matinfo, unsigned **matOverride) {

		if (matinfo.name.length() && dest->find(matinfo.name) == dest->end()) // do not overwrite stuff
        {
            if(*matOverride && **matOverride != impala::impala_noIdx())
            {
                matinfo.matidx = **matOverride;
                /*std::cerr << "override mat idx " << *nmatsOverridden << " of " << nmats << " override mats total."
                          << " diffuse=" << matinfo.mat.diffuse
                          << " specular=" << matinfo.mat.specular
                          << " emissive=" << matinfo.mat.emissive
                          << " specExp=" << matinfo.mat.specExp
                          << std::endl;*/
                *matOverride += 1; // advanced to next to-be-overridden material

            }
            std::cerr << "creating material " << matinfo.name << "\n";
            dest->insert(make_pair(matinfo.name, matinfo));
        }
        matinfo = MaterialInfo();
    }
}


void loadOBJMat(impala::Scene *scene, MatLib* dest, const std::string& path, const std::string& filename, unsigned **matOverride) {
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
            matCreate(dest, material, matOverride); //create the previous material (if it exists) and clear the material info
            cmd += 6;

            skipWS(cmd);
            material.name = endSpaceTrimmed(cmd);
        }
        else if(
            std::strncmp(cmd, "Kd", 2) == 0 || std::strncmp(cmd, "Ks", 2) == 0
            || std::strncmp(cmd, "Ka", 2) == 0)
        {
            char coeffType = *(cmd + 1);

            float color[3];
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
            impala::Texture tex;
            impala::impala_constantTexture(&tex, color[0], color[1], color[2]);
            switch (coeffType)
            {
            case 'd':
                material.mat.diffuse = impala::impala_sceneAddTexture(scene, &tex); break;
            case 'a':
                material.mat.emissive = impala::impala_sceneAddTexture(scene, &tex); break;
            case 's':
                if (material.mat.specExp < 0) material.mat.specExp = 1;
                material.mat.specular = impala::impala_sceneAddTexture(scene, &tex); break;
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
    matCreate(dest, material, matOverride);
}

void load_object_from_file(const char *path, const char *filename, unsigned flags, unsigned *matOverride, impala::Scene *scene, impala::Tris *tris)
{
    MatLib matlib;

    std::map<std::string, unsigned> materialName2Idx;
    std::set<Instruction> unsupportedEncounters;
    std::set<std::string> unknownMaterialEncounters;

    unsigned curMatIdx = 0; // the default material

    FileLine fileline;
    if(!fileline.open(std::string(path)+filename)) {
        std::cerr << "ERROR: Cannot open " << filename << std::endl;
        return;
    }

    const unsigned vertOffset = impala::impala_trisNumVertices(tris);
    const unsigned normOffset = impala::impala_trisNumNormals(tris);
    const unsigned texCoordOffset = impala::impala_trisNumTexCoords(tris);
    const unsigned noIdx = impala::impala_noIdx();

    while (!fileline.eof()) {
        fileline.nextLine();
        fileline.removeComments();
        Instruction i = fileline.fetchInstruction();
        switch (i) {
            case Obj_Vertex:
            case Obj_NormalVertex: {
                float x, y, z;
                x = fileline.fetchFloat();
                y = fileline.fetchFloat();
                z = fileline.fetchFloat();
                float w = fileline.fetchFloat(1.0f);
                if (w != 1.0f) { x/=w; y/=w; z/=w; }
                if (i == Obj_Vertex)
                    impala::impala_trisAppendVertex(tris, x, y, z);
                else
                    impala::impala_trisAppendNormal(tris, x, y, z);
                break;
            }
            case Obj_TexVertex: {
                float x, y;
                x = fileline.fetchFloat();
                y = 1-fileline.fetchFloat(0.0f);
                fileline.fetchFloat(0.0f); //u, v, w. Ignore w
                impala::impala_trisAppendTexCoord(tris, x, y);
                break;
            }
            case Obj_Face: {
                bool skiptex = (flags & IgnoreTexCoord);
                bool skipnormal = (flags & IgnoreNormals);

                Int3 v[3];

                for (int i = 0; i < 3; ++i) {
                    v[i] = fileline.fetchVertex();
                    release_assert(v[i].vidx != 0, "Error in file ", fileline.filename, ":", fileline.lineIdx, ".", fileline.pos, " : Vertex index cannot be 0");

                    if (v[i].vidx<0) v[i].vidx = impala::impala_trisNumVertices(tris) - v[i].vidx;
                    else { v[i].vidx += vertOffset-1; }

                    if (v[i].tidx == 0) skiptex=true;
                    else if (v[i].tidx<0) v[i].tidx = impala::impala_trisNumTexCoords(tris) - v[i].tidx;
                    else { v[i].tidx += texCoordOffset-1; }

                    if (v[i].nidx == 0) skipnormal=true;
                    else if (v[i].nidx<0) v[i].nidx = impala::impala_trisNumNormals(tris) - v[i].nidx;
                    else { v[i].nidx += normOffset-1; }
                }

                while(true) {
                    impala::impala_trisAppendTriangle(tris,
                                                      v[0].vidx, v[1].vidx, v[2].vidx,
                                                      skipnormal ? noIdx : v[0].nidx, skipnormal ? noIdx : v[1].nidx, skipnormal ? noIdx : v[2].nidx,
                                                      skiptex ? noIdx : v[0].tidx, skiptex ? noIdx : v[1].tidx, skiptex ? noIdx : v[2].tidx,
                                                      curMatIdx);

                    // advance to next vertex
                    v[1] = v[2];
                    const int i = 2;
                    v[i] = fileline.fetchVertex();
                    if (v[i].vidx == 0) break;

                    if (v[i].vidx<0) v[i].vidx = impala::impala_trisNumVertices(tris) - v[i].vidx;
                    else { v[i].vidx += vertOffset-1; }

                    if (v[i].tidx == 0) skiptex=true;
                    else if (v[i].tidx<0) v[i].tidx = impala::impala_trisNumTexCoords(tris) - v[i].tidx;
                    else { v[i].tidx += texCoordOffset-1; }

                    if (v[i].nidx == 0) skipnormal=true;
                    else if (v[i].nidx<0) v[i].nidx = impala::impala_trisNumNormals(tris) - v[i].nidx;
                    else { v[i].nidx += normOffset-1; }
                }
                break;
            }
            case Obj_MaterialLibrary: {
                if (!(flags & IgnoreMatLibs)) {
                    std::string libname = fileline.fetchString();
                    loadOBJMat(scene, &matlib, path, libname, &matOverride);
                }
                break;
            }
            case Obj_Material: {
                std::string matname = fileline.fetchString();
                auto i = matlib.find(matname);
                if (i != matlib.end()) {
                    // check if we already registered this material
                    auto j = materialName2Idx.find(matname);
                    if (j != materialName2Idx.end()) {
                        curMatIdx = j->second; // we have the surface index here
                    }
                    else {
                        // register material
                        curMatIdx = i->second.matidx != noIdx ? i->second.matidx : impala::impala_sceneAddMaterial(scene, &i->second.mat);
                        materialName2Idx.insert(std::make_pair(matname, curMatIdx));
                    }
                }
                else {
                    if (unknownMaterialEncounters.find(matname) == unknownMaterialEncounters.end()) {
                        std::cerr << "Warning: Material \'" << matname << "\' not found in material library at " << fileline.filename << ":" << fileline.lineIdx << "." << fileline.pos << ". Using dummy material." << std::endl;
                        unknownMaterialEncounters.insert(matname);
                    }
                    curMatIdx = 0;
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


    std::cout << "ObjLoader: Loaded "
        << impala::impala_trisNumVertices(tris)-vertOffset << " verts, "
        << impala::impala_trisNumNormals(tris)-normOffset << " normals, "
        << impala::impala_trisNumTexCoords(tris)-texCoordOffset << " texcoords, "
        << matlib.size() << " materials. "
        << std::endl;
}


}

