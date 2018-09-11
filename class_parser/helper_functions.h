#pragma once

#include <unordered_map>
#include <vector>
#include <string>
#include <sstream>
#include <iostream>
#include <functional>

#include "clang_fwd.h"
#include "qual_type_wrapper.h"

/*** NO CLANG INCLUDES ALLOWED ***/
// anything requiring a clang include must go in clang_helper_functions.h

#include "annotations.h"
#include "log.h"


#include <xl/exceptions.h>


namespace v8toolkit::class_parser {

class ClassParserException : public xl::FormattedException {
public:
    ClassParserException(xl::zstring_view format_string) :
        FormattedException(format_string)
    {
        v8toolkit::class_parser::log.error(LogT::Subjects::Exception, this->what());
    }


    /**
     * If more than one parameter provided, the first will be used as a formatting string and the other
     * parameters will be used as substitution arguments for that formatting string
     * @param format_string format string for libfmt
     * @param args substitutions for the format string
     */
    template<typename... Args>
    ClassParserException(xl::zstring_view format_string, Args&&... args) :
        FormattedException(fmt::format(format_string.c_str(), std::forward<Args>(args)...))
    {
        v8toolkit::class_parser::log.error(LogT::Subjects::Exception, this->what());
    }
};


extern std::vector<std::pair<std::string, std::string>> cpp_to_js_type_conversions;

// Any base types you want to always ignore -- v8toolkit::WrappedClassBase must remain, others may be added/changed
inline std::vector<std::string> base_types_to_ignore = {"v8toolkit::WrappedClassBase", "v8toolkit::EmptyFactoryBase"};

struct WrappedClass;

inline int print_logging = 0;

// if a static method has a name matching the key, change it to the value
extern std::unordered_map<std::string, std::string> static_method_renames;
extern std::unordered_map<std::string, int> template_instantiations;
extern std::vector<std::string> types_to_ignore_regex;
extern int matched_classes_returned;
extern std::vector<std::string> never_include_for_any_file;
//extern string header_for_every_class_wrapper_file;
extern std::vector<std::string> includes_for_every_class_wrapper_file;

bool has_wrapped_class(const CXXRecordDecl * decl);

std::string get_type_string(QualType const & qual_type,
                            const std::string & indentation = "");


enum EXPORT_TYPE {
    EXPORT_UNSPECIFIED = 0,
    EXPORT_NONE, // export nothing
    EXPORT_ALL
}; // exports everything

// log_subject - what subject to log any messages under, since this is used on different pieces of the AST
EXPORT_TYPE get_export_type(const NamedDecl * decl, LogSubjects::Subjects log_subject, EXPORT_TYPE previous = EXPORT_UNSPECIFIED);

std::string remove_reference_from_type_string(std::string const & type_string);

std::string remove_local_const_from_type_string(std::string const & type_string);
std::string make_macro_safe_comma(std::string const & input);

// take a templated type and a named set of templated substitutions and return the string representation of the resulting
//   type with the new types plugged in
std::string substitute_type(QualType const & original_type, std::unordered_map<std::string, QualType> template_types);



template<class T>
std::string join(const T & source, const std::string & between = ", ", bool leading_between = false);


std::string get_include_for_type_decl(const TypeDecl * type_decl);
std::optional<std::string> get_root_include_for_decl(const TypeDecl * type_decl);

void print_vector(const std::vector<std::string> & vec, const std::string & header = "", const std::string & indentation = "",
                  bool ignore_empty = true);

std::string get_source_for_source_range(SourceManager & sm, SourceRange source_range);

std::string get_source_for_source_range(SourceManager & sm, SourceRange source_range);

std::string get_canonical_name_for_decl(const TypeDecl * decl);

bool is_good_record_decl(const CXXRecordDecl * decl);

std::string get_include_string_for_fileid(FileID & file_id);

std::string get_method_parameters(WrappedClass & wrapped_class,
                                  const CXXMethodDecl * method,
                                  bool add_leading_comma = false,
                                  bool insert_variable_names = false,
                                  const std::string & annotation = "");


void print_specialization_info(const CXXRecordDecl * decl);



std::string get_method_string(WrappedClass & wrapped_class,
                              const CXXMethodDecl * method);




class PrintLoggingGuard {
    bool logging = false;
public:
    PrintLoggingGuard() = default;

    ~PrintLoggingGuard() {
        if (logging) {
            print_logging--;
        }
    }

    void log() {
        if (logging == true) {
            return;
        }
        print_logging++;
        logging = true;
    }
};


// joins a range of strings with commas (or whatever specified)
template<class T>
std::string join(const T & source, const std::string & between, bool leading_between) {
    if (source.empty()) {
        return "";
    }
    std::stringstream result;
    if (leading_between) {
        result << between;
    }
    bool first = true;
    for (auto & str : source) {
        if (str == "") {
            //printf("Skipping blank entry in join()\n");
            continue;
        }
        if (!first) { result << between; }
        first = false;
        result << str;
    }
    return result.str();
}

void foreach_constructor(const CXXRecordDecl * klass,
                         std::function<void(CXXConstructorDecl const *)> callback,
                         const std::string & annotation);


std::string trim_doxygen_comment_whitespace(std::string const & comment);



FullComment * get_full_comment_for_decl(Decl const * decl, bool any = false);
QualType get_type_from_dereferencing_type(QualType type);
}