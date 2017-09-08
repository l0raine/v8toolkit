#include "parsed_method.h"
#include "wrapped_class.h"

namespace v8toolkit::class_parser {


ClassFunction::TypeInfo::TypeInfo(QualType const & type, map<string, QualType> template_parameter_types) :
    template_parameter_types(template_parameter_types),
    type(type)
//name(this->type.getAsString()),
//    name(get_type_string(this->type)),
//    plain_type(get_plain_type(this->type)),
//    plain_name(get_type_string(this->plain_type))
{
//    name = regex_replace(name, std::regex("^(struct|class) "), "");
//    std::cerr << fmt::format("TypeInfo for {} stored {} template parameter types", this->type.getAsString(),
//                             this->template_parameter_types.size()) << std::endl;
}


string ClassFunction::TypeInfo::get_name() const {
    return substitute_type(this->type, this->template_parameter_types);
}


bool ClassFunction::TypeInfo::is_void() const {
    return this->get_name() == "void";
}


QualType ClassFunction::TypeInfo::get_plain_type() const {


    QualType plain_type = this->type;
    plain_type = plain_type.getNonReferenceType();//.getUnqualifiedType();
    while (!plain_type->getPointeeType().isNull()) {
        plain_type = plain_type->getPointeeType();//.getUnqualifiedType();
    }
    if (!this->type->isDependentType()) {
        return plain_type;
    }

    bool was_const_before_substitution = plain_type.isConstQualified();

    // if it's dependent, then look for the substitution
    plain_type = plain_type.getUnqualifiedType();


    plain_type = get_substitution_type_for_type(plain_type, this->template_parameter_types);


    plain_type = plain_type.getNonReferenceType();//.getUnqualifiedType();
    if (was_const_before_substitution) {
        plain_type.addConst();
    }

    while (!plain_type->getPointeeType().isNull()) {
        plain_type = plain_type->getPointeeType();//.getUnqualifiedType();
    }

    return plain_type;
}


string ClassFunction::TypeInfo::get_plain_name() const {
    return get_type_string(this->get_plain_type());
}


string ClassFunction::TypeInfo::convert_simple_typename_to_jsdoc(string simple_type_name,
                                                                 std::string const & indentation) {

    // picks off the middle namespace of things like:
    //   std::__cxx11::string as well as std::__1::vector so type names are more predictable
    simple_type_name = regex_replace(simple_type_name, regex("std::__(cxx\\d\\d|\\d)::"), "std::");

    // clang generates type names with class/struct prefixes, remove them
    simple_type_name = regex_replace(simple_type_name, regex("^(class|struct)\\s*"), "");

    // remove const/volatile
    simple_type_name = regex_replace(simple_type_name, regex("^(const\\s*|volatile\\s*)*\\s*"), "");

    std::smatch matches;

    for (auto & conversion_pattern : cpp_to_js_type_conversions) {
        for (auto & pair : cpp_to_js_type_conversions) {
            if (regex_match(simple_type_name, matches, std::regex(pair.first))) {
                std::cerr << fmt::format("{}'{}' matched {}, converting to {}",
                                         indentation,
                                         simple_type_name,
                                         pair.first,
                                         pair.second) << std::endl;
                return pair.second;
            }
        }
    }

    // no match, return unchanged
    std::cerr << fmt::format("{}returning simple type name unchanged {}",
                             indentation, simple_type_name) << std::endl;
    return simple_type_name;
}


string ClassFunction::TypeInfo::get_jsdoc_type_name(std::string const & indentation) const {
    std::cerr << fmt::format("{}converting {}", indentation, this->get_name()) << std::endl;

    vector<string> template_type_jsdoc_conversions;
    if (this->is_templated()) {

        // convert each templated type
        this->for_each_templated_type([&](QualType qualtype) {
            auto typeinfo = TypeInfo(qualtype);
            std::cerr << fmt::format("{}converting templated type {}", indentation, typeinfo.get_plain_name()) << std::endl;
            template_type_jsdoc_conversions.push_back(typeinfo.get_jsdoc_type_name(indentation + "  "));
        });

        // convert the specialized template name
        string specialized_template_name;
        if (auto specialization = dyn_cast<ClassTemplateSpecializationDecl>(this->get_plain_type_decl())) {
            if (auto spec_tmpl = specialization->getSpecializedTemplate()) {
                specialized_template_name = spec_tmpl->getQualifiedNameAsString();
                fprintf(stderr, "%sSpecialized template: %p, %s\n", indentation.c_str(), (void *) spec_tmpl,
                        specialized_template_name.c_str());
                print_vector(Annotations(spec_tmpl).get(), "specialized template annotations", "", false);
            } else {
                llvm::report_fatal_error("couldn't determine name of template being specialized");
            }
        } else {
            llvm::report_fatal_error(
                "Template being specialized couldn't be cast to class template spec decl (shouldn't happen)");
        }

        specialized_template_name = this->convert_simple_typename_to_jsdoc(specialized_template_name, indentation);


        // go through each capturing match and...
        for (size_t i = 0; i < template_type_jsdoc_conversions.size(); i++) {
            // look for $1, $2, etc in replacement and substitute in the matching position
            specialized_template_name = std::regex_replace(specialized_template_name,
                                                           std::regex(fmt::format("\\${}", i + 1)),
                                                           template_type_jsdoc_conversions[i]);
        }
        std::cerr << fmt::format("{}final jsdoc conversion: {} =? {}",
                                 indentation, this->get_plain_name(), specialized_template_name)
                  << std::endl;
        return specialized_template_name;
    }
        // Handle non-templated types
    else {
        return this->convert_simple_typename_to_jsdoc(this->plain_without_const().get_name(), indentation);
    }
}

bool ClassFunction::TypeInfo::is_const() const {
    return this->get_plain_type().isConstQualified();
}


ClassFunction::TypeInfo ClassFunction::TypeInfo::plain_without_const() const {
    QualType non_const = this->get_plain_type();
    non_const.removeLocalConst();
    return TypeInfo(non_const, this->template_parameter_types);
}


CXXRecordDecl const * ClassFunction::TypeInfo::get_plain_type_decl() const {
    auto decl = this->get_plain_type()->getAsCXXRecordDecl();
    if (decl == nullptr) {
        return nullptr;
    }
    return dyn_cast<ClassTemplateSpecializationDecl>(decl);
}


// Whether the plain type is templated or not, not whether the original type corresponds to a templated function type parameter
bool ClassFunction::TypeInfo::is_templated() const {
    // what about CXXMethodDecl::isFunctionTemplateSpecialization ?
    auto decl = this->get_plain_type_decl();
    if (decl == nullptr) {
        return false;
    }
    if (dyn_cast<ClassTemplateSpecializationDecl>(decl) != nullptr) {
        return true;
    } else {
        return false;
    }
}


void ClassFunction::TypeInfo::for_each_templated_type(std::function<void(QualType)> callback) const {
    if (auto specialization_decl = dyn_cast<ClassTemplateSpecializationDecl>(
        this->get_plain_type()->getAsCXXRecordDecl())) {

        // go through the template args
        auto & template_arg_list = specialization_decl->getTemplateArgs();
        for (decltype(template_arg_list.size()) i = 0; i < template_arg_list.size(); i++) {
            auto & arg = template_arg_list[i];

            // this code only cares about types, so skip non-type template arguments
            if (arg.getKind() != clang::TemplateArgument::Type) {
                continue;
            }
            auto template_arg_qual_type = arg.getAsType();
            if (template_arg_qual_type.isNull()) {
                if (print_logging) cerr << "qual type is null" << endl;
                continue;
            }
            callback(template_arg_qual_type);
        }
    } else {
        if (print_logging)
            cerr << "Not a template specializaiton type " << this->get_plain_type().getAsString() << endl;
    }
}


DataMember::DataMember(WrappedClass & wrapped_class,
                       WrappedClass & declared_in,
                       FieldDecl * field_decl) :
    wrapped_class(wrapped_class),
    declared_in(declared_in),
    short_name(field_decl->getNameAsString()),
    long_name(field_decl->getQualifiedNameAsString()),
    type(field_decl->getType()),
    field_decl(field_decl),
    annotations(this->field_decl) {
    wrapped_class.add_member_name(this->short_name);
    wrapped_class.declaration_count++;

    update_wrapped_class_for_type(wrapped_class, this->type.get_plain_type());

    // the member will be wrapped as const if the actual data type is const or there's an attribute saying it should be const
    this->is_const = this->type.is_const() || annotations.has(V8TOOLKIT_READONLY_STRING);

}


string DataMember::get_js_stub() {

    stringstream result;

    result << fmt::format(" * @property {{{}}} {} \n", this->type.get_jsdoc_type_name(), this->short_name);

    return result.str();
}

string DataMember::get_bindings() {
    stringstream result;

    if (this->is_const) {
        result << fmt::format("    class_wrapper.add_member_readonly<{}, {}, &{}>(\"{}\");\n",
                              this->type.get_name(),
                              this->declared_in.class_name, this->long_name, this->short_name);

    } else {
        result << fmt::format("    class_wrapper.add_member<{}, {}, &{}>(\"{}\");\n",
                              this->type.get_name(),
                              this->declared_in.class_name, this->long_name, this->short_name);
    }

    return result.str();
}

string ClassFunction::ParameterInfo::generate_js_stub() {
    stringstream result;

    if (this->default_value != "") {
        result << fmt::format("     * @param {{{}}} [{} = {}] {}\n", this->type.get_jsdoc_type_name(),
                              this->name,
                              this->default_value,
                              this->description);
    } else {
        result << fmt::format("     * @param {{{}}} {}\n", this->type.get_jsdoc_type_name(), this->name,
                              this->description);
    }
    return result.str();
}

ClassFunction::ParameterInfo::ParameterInfo(ClassFunction & method, int position, ParmVarDecl const * parameter_decl,
                                            CompilerInstance & compiler_instance) :
    method(method),
    compiler_instance(compiler_instance),
    parameter_decl(parameter_decl),
    position(position),
    type(parameter_decl->getType(), method.template_parameter_types) {
    std::cerr
        << fmt::format("In Parameter Info, method has {} template parameter types that were passed in to TypeInfo",
                       method.template_parameter_types.size()) << std::endl;
    //std::cerr << fmt::format("parameterinfo constructor: parsing parameter {}", name) << std::endl;
    // set the name, give placeholder name if unnamed
    //std::cerr << fmt::format("1") << std::endl;
    this->name = this->parameter_decl->getNameAsString();
    //std::cerr << fmt::format("2") << std::endl;
    if (this->name == "") {
        //std::cerr << fmt::format("3") << std::endl;
        this->name = fmt::format("unspecified_position_{}", this->position);

        data_warning(fmt::format("class {} method {} parameter index {} has no variable name",
                                 this->method.wrapped_class.name_alias, this->method.name, this->position));
    }

    // set default argument or "" if none
    if (parameter_decl->hasDefaultArg()) {
        auto default_argument = parameter_decl->getDefaultArg();
        if (default_argument != nullptr) {
            auto source_range = default_argument->getSourceRange();
            if (source_range.isValid()) {

                auto source = get_source_for_source_range(compiler_instance.getSourceManager(), source_range);

                // certain default values return the = sign, others don't.  specifically  "= {}" comes back with the =, so strip it off
                // is this a clang bug?
                this->default_value = std::regex_replace(source, std::regex("^\\s*=\\s*"), "");

                if (this->default_value == "{}") {

                    this->default_value = fmt::format("{}{{}}", this->type.plain_without_const().get_name());
                }

            } else {

            }
        } else {
        }
    } else {
        this->default_value = "";
    }

}


ClassFunction::ClassFunction(WrappedClass & wrapped_class,
                             CXXMethodDecl const * method_decl,
                             std::map<string, QualType> const & template_parameter_types,
                             FunctionTemplateDecl const * function_template_decl) :
    compiler_instance(wrapped_class.compiler_instance),
    return_type(method_decl->getReturnType(), template_parameter_types),
    method_decl(method_decl),
    name(method_decl->getQualifiedNameAsString()),
    js_name(method_decl->getNameAsString()),
    wrapped_class(wrapped_class),
    is_virtual(method_decl->isVirtual()),
    annotations(this->method_decl),
    template_parameter_types(template_parameter_types),
    function_template_decl(function_template_decl) {
    std::cerr << fmt::format("ClassFunction for {} got {} template substitutions", this->name,
                             this->template_parameter_types.size()) << std::endl;
    // check to see if there's a name annotation on the method giving it a different JavaScript name
    auto annotated_custom_name = annotations.get_regex(
        "^" V8TOOLKIT_USE_NAME_PREFIX "(.*)$");
    if (!annotated_custom_name.empty()) {
        std::cerr << fmt::format("Overriding method name {} => {}", this->js_name, annotated_custom_name[0])
                  << std::endl;
        this->js_name = annotated_custom_name[0];
        std::cerr << fmt::format("short name is now {}", this->js_name) << std::endl;
    } else {
        std::cerr << fmt::format("not overriding method name {}", this->js_name) << std::endl;
    }

    this->wrapped_class.declaration_count++;

    std::cerr << fmt::format("***** Parsing method {}", this->name) << std::endl;

    update_wrapped_class_for_type(this->wrapped_class, this->return_type.get_plain_type());

    auto parameter_count = method_decl->getNumParams();
    for (int i = 0; i < parameter_count; i++) {
        std::cerr << fmt::format("ParsedMethod constructor - parsing parameter {}", i) << std::endl;
        parameters.emplace_back(*this, i, method_decl->getParamDecl(i), this->compiler_instance);

        // make sure the wrapped class has includes for all the types in the method
        update_wrapped_class_for_type(this->wrapped_class, this->parameters.back().type.get_plain_type());
    }


    // get the comment associated with the method and if there is one, parse it
    std::cerr << fmt::format("Parsing doxygen comments") << std::endl;
    FullComment * comment = this->compiler_instance.getASTContext().getCommentForDecl(this->method_decl, nullptr);
    if (comment != nullptr) {

        auto comment_text = get_source_for_source_range(
            this->compiler_instance.getPreprocessor().getSourceManager(), comment->getSourceRange());

        cerr << "FullComment: " << comment_text << endl;

        // go through each portion (child) of the full commetn
        int j = 0;
        for (auto i = comment->child_begin(); i != comment->child_end(); i++) {
            std::cerr << fmt::format("looking at child comment {}", ++j) << std::endl;
            auto child_comment_source_range = (*i)->getSourceRange();
            if (child_comment_source_range.isValid()) {

                auto child_comment_text = get_source_for_source_range(
                    this->compiler_instance.getPreprocessor().getSourceManager(),
                    child_comment_source_range);

                cerr << "Child comment kind: " << (*i)->getCommentKind() << ": " << child_comment_text << endl;

                // if the child comment is a param command comment (describes a parameter)
                if (auto param_command = dyn_cast<ParamCommandComment>(*i)) {
                    cerr << "Is ParamCommandComment" << endl;
                    if (param_command == nullptr) {
                        std::cerr << fmt::format("THIS CANT BE RIGHT") << std::endl;
                    }
                    std::cerr << fmt::format("param name aswritten: {}", param_command->getParamNameAsWritten().str())
                              << std::endl;

                    // cannot use getParamName() because it crashes if the name doesn't match a parameter
                    auto command_param_name = param_command->getParamNameAsWritten().str();
                    std::cerr << fmt::format("got command param name {}", command_param_name) << std::endl;

                    ParameterInfo * matching_parameter_info_ptr = nullptr;
                    for (auto & parameter : this->parameters) {
                        std::cerr << fmt::format("comparing {} against {}", command_param_name, parameter.name)
                                  << std::endl;
                        if (command_param_name == parameter.name) {
                            std::cerr << fmt::format("found match!") << std::endl;
                            matching_parameter_info_ptr = &parameter;
                            break;
                        }
                    }
//                    auto matching_param_iterator =
//                        std::find_if(parameters.begin(), parameters.end(),
//                                     [&command_param_name](auto &param) {
//                                         return command_param_name == param.name;
//                                     });

                    std::cerr << fmt::format("found parameter (not matching .end()) {}",
                                             matching_parameter_info_ptr != nullptr) << std::endl;
                    std::cerr << fmt::format("has param name?  {}", param_command->hasParamName()) << std::endl;
                    if (param_command->hasParamName() && matching_parameter_info_ptr != nullptr) {

                        auto & param_info = *matching_parameter_info_ptr;
                        if (param_command->getParagraph() != nullptr) {
                            param_info.description = get_source_for_source_range(
                                this->compiler_instance.getPreprocessor().getSourceManager(),
                                param_command->getParagraph()->getSourceRange());
                        }
                    } else {
                        data_warning(
                            fmt::format("method parameter comment name doesn't match any parameter {}",
                                        command_param_name));
                    }
                } else {
                    cerr << "is not param command comment" << endl;
                }
            }
        }
    } else {
        cerr << "No comment on " << method_decl->getNameAsString() << endl;
    }

}



//
//std::string ClassFunction::get_js_stub() {
//
//    string indentation = "    ";
//    stringstream result;
//
//    string method_description;
//
//    result << fmt::format("{}/**\n", indentation);
//
////    std::cerr << fmt::format("looking through {} parameters", this->parameters.size()) << std::endl;
//    for (auto &param : this->parameters) {
//      if (param.default_value != "") {
//	result << fmt::format("{} * @param {{{}}} [{} = {}] {}\n", indentation, param.type.get_jsdoc_type_name(),
//			      param.name,
//			      param.default_value,
//			      param.description);
//      } else {
//	result << fmt::format("{} * @param {{{}}} {}\n", indentation, param.type.get_jsdoc_type_name(), param.name,
//			      param.description);
//      }
//    }
//
//    result << fmt::format("{} * @return {{{}}} {}\n", indentation, this->return_type.get_jsdoc_type_name(),
//                          this->return_type_comment);
//    result << fmt::format("{} */\n", indentation);
//
//    result << fmt::format("{}{}{}(", indentation, this->is_static ? "static " : "", this->short_name);
//
//    bool first_parameter = true;
//    for (auto &param : parameters) {
//        if (!first_parameter) {
//            result << ", ";
//        }
//        first_parameter = false;
//        result << fmt::format("{}", param.name);
//    }
//    result << fmt::format("){{}}\n\n");
//
//    return result.str();
//}


string ClassFunction::get_default_argument_tuple_string() const {
    stringstream types;
    stringstream values;
    bool first_default_argument = true;
    for (auto & param : this->parameters) {


        // this should go away once there's proper support
        if (std::regex_search(param.type.get_plain_type().getAsString(),
                              std::regex("^(class|struct)?\\s*std::function"))) {
            std::cerr << fmt::format("Cannot handle std::function default parameters yet -- skipping") << std::endl;
            continue;
        }

        if (param.default_value == "") {
            assert(first_default_argument); // if it's not true then there's a gap somehow
            continue;
        }
        if (!first_default_argument) {
            types << ", ";
            values << ", ";
        }
        first_default_argument = false;

        //types << param.type.name; // still has const and references on it, which doesn't work well for tuples
        std::cerr << fmt::format("ORIG: {}", param.type.get_name()) << std::endl;
        std::cerr << fmt::format("POST: {}", remove_local_const_from_type_string(
            remove_reference_from_type_string(param.type.get_name()))) << std::endl;
        types << remove_local_const_from_type_string(remove_reference_from_type_string(param.type.get_name()));


        // this may have a problem with using type names not visible outside where the default argument is specified
        // may need to substitute the type name instead if it's not a constant and is instead some sort of name
        values << param.default_value;
    }

    return fmt::format("std::tuple<{}>({})", types.str(), values.str());
}

string ClassFunction::get_return_and_class_and_parameter_types_string() const {
    stringstream result;

    result << fmt::format("{}, {}", this->return_type.get_name(), this->wrapped_class.class_name);
    if (!this->parameters.empty()) {
        result << ", ";
        result << this->get_parameter_types_string();
    }

    return result.str();
}

string ClassFunction::get_return_and_parameter_types_string() const {
    stringstream result;

    result << fmt::format("{}", this->return_type.get_name());
    if (!this->parameters.empty()) {
        result << ", ";
        result << this->get_parameter_types_string();
    }

    return result.str();
}

string ClassFunction::get_parameter_types_string() const {
    stringstream result;
    bool first_result = true;
    for (auto & parameter : this->parameters) {
        if (!first_result) {
            result << ", ";
        }
        first_result = false;
        result << fmt::format("{}", parameter.type.get_name());
    }

    return result.str();
}

string ClassFunction::get_js_input_parameter_string() const {
    stringstream result;
    bool first_result = true;
    for (auto & parameter : this->parameters) {
        if (!first_result) {
            result << ", ";
        }
        first_result = false;
        result << fmt::format("{}", parameter.name);
    }

    return result.str();
}


string ClassFunction::get_signature_string() {
    stringstream result;
    result << this->name << "(";

    bool first = true;
    for (auto & p : this->parameters) {
        if (!first) {
            result << ",";
        }
        first = false;
        result << p.type.get_name();
    }

    result << ")";
    return result.str();
}

MemberFunction::MemberFunction(WrappedClass & wrapped_class, CXXMethodDecl const * method_decl,
                               map<string, QualType> const & map, FunctionTemplateDecl const * function_template_decl) :
    ClassFunction(wrapped_class, method_decl, map, function_template_decl) {
    wrapped_class.add_member_name(this->js_name);

    for (auto a = method_decl->attr_begin(); a != method_decl->attr_end(); a++) {
        std::cerr << fmt::format("on function {} looking at attribute {}", this->name, (*a)->getSpelling())
                  << std::endl;
        if ((*a)->getKind() == attr::Kind::Final) {
            std::cerr << fmt::format("setting is_virtual_final = true") << std::endl;
            this->is_virtual_final = true;
        }
        if ((*a)->getKind() == attr::Kind::Override) {
            std::cerr << fmt::format("setting is_virtual_override= true") << std::endl;

            this->is_virtual_override = true;
        }
    };
}

StaticFunction::StaticFunction(WrappedClass & wrapped_class, CXXMethodDecl const * method_decl,
                               map<string, QualType> const & map, FunctionTemplateDecl const * function_template_decl) :
    ClassFunction(wrapped_class, method_decl, map, function_template_decl) {
    wrapped_class.add_static_name(this->js_name);

    if (static_method_renames.find(this->js_name) != static_method_renames.end()) {
        this->js_name = static_method_renames[this->js_name];
    }
}


ConstructorFunction::ConstructorFunction(WrappedClass & wrapped_class, CXXConstructorDecl const * constructor_decl) :
    ClassFunction(wrapped_class, constructor_decl),
    constructor_decl(constructor_decl) {

    cerr << "About to get full source for constructor in " << wrapped_class.name_alias << endl;
    auto full_source_loc = FullSourceLoc(constructor_decl->getLocation(),
                                         this->compiler_instance.getSourceManager());
    fprintf(stderr, "%s constructor Decl at line %d, file id: %d\n",
            wrapped_class.name_alias.c_str(),
            full_source_loc.getExpansionLineNumber(),
            full_source_loc.getFileID().getHashValue());

    // this should be moved to ClassFunction
//    Annotations constructor_annotations(constructor_decl);
//    auto constructor_name_annotation = constructor_annotations.get_regex(V8TOOLKIT_CONSTRUCTOR_PREFIX "(.*)");
//    // fprintf(stderr,"Got %d annotations on constructor\n", (int)constructor_annotations.size());
//    std::string constructor_name = wrapped_class.name_alias;
//    if (!constructor_name_annotation.empty()) {
//        constructor_name = constructor_name_annotation[0];
//    }


    if (std::find(used_constructor_names.begin(), used_constructor_names.end(), this->js_name) !=
        used_constructor_names.end()) {
        data_error(
            fmt::format("Error: because duplicate JS constructor function name: {} in class {}",
                        this->js_name.c_str(), wrapped_class.name_alias));
        for (auto & name : used_constructor_names) {
            cerr << (fmt::format("Already used constructor name: {}", name)) << endl;
        }
    } else {
        cerr << fmt::format("for {}, wrapping constructor {}", wrapped_class.name_alias,
                            this->js_name) << endl;
        used_constructor_names.push_back(this->js_name);
    }
}


string ConstructorFunction::generate_js_bindings() {
    stringstream result;
    result << fmt::format("    class_wrapper.add_constructor<{}>(\"{}\", isolate, {});",
                          this->get_parameter_types_string(), this->js_name, this->get_default_argument_tuple_string())
           << endl;

    return result.str();
}

string MemberFunction::generate_js_bindings() {
    stringstream result;
    if (OO_Call == method_decl->getOverloadedOperator()) {
        result << fmt::format("    class_wrapper.make_callable<{}>(&{});\n",
                              this->get_return_and_class_and_parameter_types_string(), this->name);

    } else {
        std::cerr << fmt::format("about to add_method for {}", this->name) << std::endl;
        result << fmt::format("    class_wrapper.add_method<{}>(\"{}\", &{}, {});\n",
                              this->get_return_and_class_and_parameter_types_string(),
                              this->js_name,
                              this->name,
                              this->get_default_argument_tuple_string());
    }
    return result.str();
}


string MemberFunction::generate_js_stub() {
    stringstream result;
    result << fmt::format("    /**") << endl;
    for (auto & parameter : this->parameters) {
        result << parameter.generate_js_stub();
    }
    if (!this->return_type.is_void()) {
        result << fmt::format("     * @return {{{}}}", this->return_type.get_jsdoc_type_name()) << endl;
    }

    result << fmt::format("     */") << endl;
    result << fmt::format("    {}({}){{}}", this->js_name, this->get_js_input_parameter_string());

    return result.str();
}


string MemberFunction::generate_bidirectional() {
    stringstream result;

    return result.str();

}


string StaticFunction::generate_js_bindings() {
    stringstream result;
    result << fmt::format("    class_wrapper.add_static_method<{}>(\"{}\", &{}, {});\n",
                          this->get_return_and_parameter_types_string(), this->js_name, this->name,
                          this->get_default_argument_tuple_string());
    return result.str();

}

string StaticFunction::generate_js_stub() {
    stringstream result;
    result << fmt::format("    /**") << endl;
    for (auto & parameter : this->parameters) {
        result << parameter.generate_js_stub();
    }
    if (!this->return_type.is_void()) {
        result << fmt::format("     * @return {{{}}}", this->return_type.get_jsdoc_type_name()) << endl;
    }
    result << fmt::format("     */") << endl;

    result << fmt::format("    static {}({}){{}}", this->js_name, this->get_js_input_parameter_string()) << endl;
    result << endl;

    return result.str();

}

string ConstructorFunction::generate_js_stub() {
    stringstream result;

    result << fmt::format("    /**") << endl;
    for (auto & parameter : this->parameters) {
        result << parameter.generate_js_stub();
    }
    result << fmt::format("     */") << endl;

    result << fmt::format("    constructor({}){{}}\n\n", this->get_js_input_parameter_string());

    return result.str();

}


}