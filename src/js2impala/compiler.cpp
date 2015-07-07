#include <string>
#include <vector>
#include <rapidjson/rapidjson.h>
#include <rapidjson/document.h>
#include "compiler.h"

namespace js = rapidjson;

class Compiler {
public:
    Compiler(std::ostream& out, std::ostream& err)
        : out_(out), err_(err), indent_(0), tab_("    ")
    {}

    void compile(const js::Document& doc) {
        if (expect_object_member(doc, "body")) {
            compile_program(doc["body"]);
        }
    }

private:
    typedef std::vector<std::string> MemberList;

    template <typename T, typename... Args>
    void error(T t, Args... args) { err_ << t; error(args...); }
    template <typename T>
    void error(T t) { err_ << t << std::endl; }

    template <typename S, typename F>
    void expect_array(const js::Value& v, S sep, F f) {
        if (v.IsArray()) {
            for (size_t i = 0; i < v.Size(); i++) {
                f(v[i]);
                if (i != v.Size() - 1) sep();
            }
        } else {
            error("value is not an array");
        }
    }

    bool expect_member(const js::Value& v, const std::string& name) {
        if (!v.HasMember(name.c_str())) {
            error("\'", name, "\' is not a member");
            return false;
        }
        return true;
    }

    bool expect_object_member(const js::Value& v, const std::string& name) {
        return expect_object(v) && expect_member(v, name);
    }

    bool expect_object(const js::Value& v) {
        if (!v.IsObject()) {
            error("value is not an object");
            return false;
        }
        return true;
    }

    bool expect_string(const js::Value& v) {
        if (!v.IsString()) {
            error("value is not a string");
            return false;
        }
        return true;
    }

    bool expect_int(const js::Value& v) {
        if (!v.IsInt()) {
            error("value is not an integer");
            return false;
        }
        return true;
    }

    bool expect_boolean(const js::Value& v) {
        if (!v.IsBool()) {
            error("value is not an boolean");
            return false;
        }
        return true;
    }

    void compile_program(const js::Value& prg) {
        expect_array(prg, [&] { new_line(); }, [&] (const js::Value& value) {
            compile_decl(value);
        });
    }

    void compile_decl(const js::Value& decl) {
        if (expect_object_member(decl, "type")) {
            const js::Value& type = decl["type"];
            if (expect_string(type)) {
                if (!strcmp(type.GetString(), "FunctionDeclaration"))
                    compile_function_decl(decl);
                else if (!strcmp(type.GetString(), "VariableDeclaration"))
                    compile_variable_decl(decl);
                else
                    error("unsupported declaration");
            }
        }
    }

    void compile_function_decl(const js::Value& fn_decl) {
        write("fn ");
        if (expect_member(fn_decl, "id"))
            compile_id(fn_decl["id"]);

        write("(");
        if (expect_member(fn_decl, "params")) {
            expect_array(fn_decl["params"], [&] { write(", "); }, [&] (const js::Value& value) {
                compile_param(value);
            });
        }
        write(") -> ");

        if (expect_member(fn_decl, "extra")) {
            const js::Value& extra = fn_decl["extra"];
            if (expect_member(extra, "returnInfo")) {
                compile_type(extra["returnInfo"]);
            }
        }

        write(" ");

        if (expect_member(fn_decl, "body"))
            compile_block_stmt(fn_decl["body"]);
    }

    void compile_variable_decl(const js::Value& var_decl) {
        if (expect_member(var_decl, "declaration")) {
            expect_array(var_decl["declarations"], [&] () { new_line(); }, [&] (const js::Value& var) {
                compile_var(var);
            });
        }
    }

    void compile_var(const js::Value& var) {
        if (expect_object(var)) {
            write("let ");
            if (expect_member(var, "id"))
                compile_id(var["id"]);
            write(": ");
            if (expect_member(var, "extra"))
                compile_type(var["extra"]);
            if (var.HasMember("init")) {
                write(" = ");
                compile_expr(var["init"]);
            }
            write(";");
        }
    }

    void compile_param(const js::Value& param) {
        if (expect_object(param)){
            if (expect_member(param, "name")) {
                const js::Value& name = param["name"];
                if (expect_string(name))
                    write(name.GetString());
            }

            write(": ");

            if (expect_member(param, "extra"))
                compile_type(param["extra"]);
        }
    }

    void compile_stmt(const js::Value& stmt) {
        if (expect_object_member(stmt, "type")) {
            const js::Value& type = stmt["type"];
            if (expect_string(type)) {
                if (!strcmp(type.GetString(), "BlockStatement"))
                    compile_block_stmt(stmt);
                else if (!strcmp(type.GetString(), "ReturnStatement"))
                    compile_return_stmt(stmt);
                else
                    error("unsupported statement");
            }
        }
    } 

    void compile_block_stmt(const js::Value& block) {
        write("{");
        indent();
        new_line();
        if (expect_member(block, "body")) {
            expect_array(block["body"], [&] () { new_line(); }, [&] (const js::Value& stmt) {
                compile_stmt(stmt);
            });
        }
        unindent();
        new_line();
        write("}");
    }

    void compile_return_stmt(const js::Value& ret) {
        write("return(");
        if (expect_member(ret, "argument"))
            compile_expr(ret["argument"]);
        write(");");
    }

    void compile_expr(const js::Value& expr) {
        if (expect_object_member(expr, "type")) {
            const js::Value& type = expr["type"];
            if (expect_string(type)) {
                if (!strcmp(type.GetString(), "Identifier"))
                    compile_id(expr);
                else if (!strcmp(type.GetString(), "Literal"))
                    compile_literal(expr);
                else if (!strcmp(type.GetString(), "CallExpression"))
                    compile_call(expr);
                else if (!strcmp(type.GetString(), "BinaryExpression"))
                    compile_binop(expr);
                else if (!strcmp(type.GetString(), "NewExpression"))
                    compile_new(expr);
                else
                    error("unsupported expression");
            }
        }
    }

    void compile_id(const js::Value& id) {
        if (expect_object_member(id, "name")) {
            const js::Value& name = id["name"];
            if (expect_string(name))
                write(name.GetString());        
        }
    }

    void compile_binop(const js::Value& binop) {
        if (expect_object(binop) &&
            expect_member(binop, "extra")) {
            const js::Value& extra = binop["extra"];

            if (is_builtin_type(extra)) {
                // For built-in types, expression can be translated as-is
                if (expect_member(binop, "left")) compile_expr(binop["left"]);
                if (expect_member(binop, "operator")) {
                    const js::Value& op = binop["operator"];
                    if (expect_string(op)) write(" ", op.GetString(), " ");
                }
                if (expect_member(binop, "right")) compile_expr(binop["right"]);
            } else {
                // For other types, we need to create function calls
                std::string prefix;
                if (expect_member(extra, "kind")) {
                    const js::Value& kind = extra["kind"];
                    if (expect_string(kind)) {
                        if (!strcmp(kind.GetString(), "float2"))
                            prefix = "vec2";
                        else if (!strcmp(kind.GetString(), "float3"))
                            prefix = "vec3";
                        else if (!strcmp(kind.GetString(), "float4"))
                            prefix = "vec4";
                        else
                            error("unsupported kind for binop");
                    }
                }

                std::string suffix;
                if (expect_member(binop, "operator")) {
                    const js::Value& op = binop["operator"];
                    if (expect_string(op)) {
                        if (!strcmp(op.GetString(), "+"))
                            suffix = "add";
                        else if (!strcmp(op.GetString(), "-"))
                            suffix = "sub";
                        else if (!strcmp(op.GetString(), "*"))
                            suffix = "mul";
                        else if (!strcmp(op.GetString(), "/"))
                            suffix = "div";
                        else
                            error("unsupported operator for binop");
                    }
                }

                write(prefix, " ", suffix, "(");
                if (expect_member(binop, "left")) compile_expr(binop["left"]);
                write(", ");
                if (expect_member(binop, "right")) compile_expr(binop["right"]);
                write(")");
            }
        }
    }

    void compile_literal(const js::Value& lit) {
        if (expect_member(lit, "value") && expect_member(lit, "extra")) {
            const js::Value& value = lit["value"];
            if (value.IsInt()) {
                if (is_floating_point(lit["extra"]))
                    write(value.GetInt(), ".0f");
                else
                    write(value.GetInt());
            } else if (value.IsBool()) write(value.GetBool() ? "true" : "false");
            else if (value.IsDouble()) write(value.GetDouble(), "f");
            else error("unsupported literal");
        }
    }

    void compile_call(const js::Value& call) {
        if (expect_member(call, "callee")) compile_expr(call["callee"]);
        write("(");
        if (expect_member(call, "arguments")) {
            expect_array(call["arguments"], [&] () { write(", "); }, [&] (const js::Value& arg) {
                compile_expr(arg);
            });
        }
        write(")");
    }

    void compile_new(const js::Value& call) {
        if (expect_member(call, "callee"))
            compile_expr(call["callee"]);
        write("{");
        if (expect_member(call, "arguments") &&
            expect_member(call, "extra")) {
            const MemberList& members = member_list(call["extra"]);
            int m = 0;
            expect_array(call["arguments"], [&] () { write(", "); }, [&] (const js::Value& arg) {
                write(members[m++], ": ");
                compile_expr(arg);
            });
        }
        write("}");
    }

    void compile_type(const js::Value& extra) {
        if (expect_object_member(extra, "type")) {
            const js::Value& type = extra["type"];
            if (expect_string(type)) {
                if (!strcmp(type.GetString(), "object")) {
                    if (expect_member(extra, "kind")) {
                        const js::Value& kind = extra["kind"];
                        if (expect_string(kind)) {
                            if (!strcmp(kind.GetString(), "float2"))
                                write("Vec2");
                            else if (!strcmp(kind.GetString(), "float3"))
                                write("Vec3");
                            else if (!strcmp(kind.GetString(), "float4"))
                                write("Vec4");
                            else
                                error("unsupported kind");
                        }
                    }
                } else if (!strcmp(type.GetString(), "number"))
                    write("f32");
                else if (!strcmp(type.GetString(), "int"))
                    write("i32");
                else if (!strcmp(type.GetString(), "boolean"))
                    write("bool");
                else
                    write("unsupported type");
            }
        }
    }

    bool is_builtin_type(const js::Value& extra) {
        if (expect_object_member(extra, "type")) {
            const js::Value& type = extra["type"];
            if (expect_string(type)) {
                if (!strcmp(type.GetString(), "object"))
                    return false;
                else if (!strcmp(type.GetString(), "number"))
                    return true;
                else if (!strcmp(type.GetString(), "int"))
                    return true;
                else if (!strcmp(type.GetString(), "boolean"))
                    return true;
                else
                    error("unsupported type");
            }
        }
        return false;
    }

    bool is_floating_point(const js::Value& extra) {
        if (expect_object_member(extra, "type")) {
            const js::Value& type = extra["type"];
            if (expect_string(type) && !strcmp(type.GetString(), "number"))
                return true;
        }
        return false;
    }

    MemberList member_list(const js::Value& extra) {
        if (expect_object_member(extra, "type")) {
            const js::Value& type = extra["type"];
            if (expect_string(type) &&
                !strcmp(type.GetString(), "object") &&
                expect_member(extra, "kind")) {
                const js::Value& kind = extra["kind"];
                if (expect_string(kind)) {
                    if (!strcmp(kind.GetString(), "float2"))
                        return MemberList({"x", "y"});
                    else if (!strcmp(kind.GetString(), "float3"))
                        return MemberList({"x", "y", "z"});
                    else if (!strcmp(kind.GetString(), "float4"))
                        return MemberList({"x", "y", "z", "w"});
                    else
                        error("unsupported kind");
                }
            }
            error("unsupported type");
        }
        return MemberList();
    }

    template <typename T, typename... Args>
    void write(T t, Args... args) { out_ << t; write(args...); }
    template <typename T>
    void write(T t) { out_ << t; }
    void indent() { indent_++; }
    void unindent() { indent_--; }
    void new_line() { out_ << "\n"; tabulation(); }
    void tabulation () { for (int i = 0; i < indent_; i++) out_ << tab_; }

    int indent_;
    const std::string tab_;
    std::ostream& out_, &err_;
};

std::string read_stream(std::istream& stream) {
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

void compile(std::istream& in, std::ostream& out, std::ostream& err) {
    Compiler c(out, err);
    js::Document doc;
    doc.Parse(read_stream(in).c_str());
    c.compile(doc);
}

