#include <assert.h>
#include <sstream>
#include <set>
#include <v8toolkit/v8toolkit.h>

#include "v8toolkit/v8helpers.h"





void* operator new[](size_t size, const char* pName, int flags, unsigned debugFlags, const char* file, int line)
{
    return malloc(size);
}

void* operator new[](size_t size, size_t alignment, size_t alignmentOffset, const char* pName, int flags, unsigned debugFlags, const char* file, int line)
{
    return malloc(size);
}

namespace v8toolkit {



MethodAdderData::MethodAdderData() = default;
MethodAdderData::MethodAdderData(std::string const & method_name,
                                 StdFunctionCallbackType const & callback) :
    method_name(method_name),
    callback(callback)
{}


/**
* Returns a string with the given stack trace and a leading and trailing newline
* @param stack_trace stack trace to return a string representation of
* @return string representation of the given stack trace
*/
std::string get_stack_trace_string(v8::Local<v8::StackTrace> stack_trace) {
    std::stringstream result;
    result << std::endl;
    int frame_count = stack_trace->GetFrameCount();
    for (int i = 0; i < frame_count; i++) {
        auto frame = stack_trace->GetFrame(i);
        result << fmt::format("{}:{} {}",
                              *v8::String::Utf8Value(frame->GetScriptName()),
                              frame->GetLineNumber(),
                              *v8::String::Utf8Value(frame->GetFunctionName())) << std::endl;
    }
    return result.str();
}



using namespace literals;

int get_array_length(v8::Isolate * isolate, v8::Local<v8::Array> array) {
    auto context = isolate->GetCurrentContext();
    return array->Get(context, "length"_v8).ToLocalChecked()->Uint32Value();
}


int get_array_length(v8::Isolate * isolate, v8::Local<v8::Value> array_value)
{
    if(array_value->IsArray()) {
        return get_array_length(isolate, v8::Local<v8::Array>::Cast(array_value));
    } else {
        // TODO: probably throw?
        throw V8AssertionException(isolate, "non-array passed to v8toolkit::get_array_length");
    }
}


int get_array_length(v8::Local<v8::Value> array_value) {
    auto isolate = v8::Isolate::GetCurrent();
    return get_array_length(isolate, array_value);
}



void set_global_object_alias(v8::Local<v8::Context> context, std::string const & alias_name)
{
    auto global_object = context->Global();
    (void)global_object->Set(context, v8::String::NewFromUtf8(context->GetIsolate(), alias_name.c_str()), global_object);

}


std::string get_type_string_for_value(v8::Local<v8::Value> value) {

    // object, function, undefined, string, number, boolean, symbol
    if (value->IsUndefined()) {
        return "undefined";
    } else if (value->IsFunction()) {
        return "function";
    } else if (value->IsString()) {
        return "string";
    } else if (value->IsNumber()) {
        return "number";
    } else if (value->IsBoolean()) {
        return "boolean";
    } else if (value->IsSymbol()) {
        return "symbol";
    } else if (value->IsObject()) {
        return "object";
    } else if (value->IsArray()) {
        return "array";
    } else {
        throw InvalidCallException("Unknown V8 object type to get type string from");
    }
}


void print_v8_value_details(v8::Local<v8::Value> local_value) {

    auto value = *local_value;

    std::cout << "undefined: " << value->IsUndefined() << std::endl;
    std::cout << "null: " << value->IsNull() << std::endl;
    std::cout << "true: " << value->IsTrue() << std::endl;
    std::cout << "false: " << value->IsFalse() << std::endl;
    std::cout << "name: " << value->IsName() << std::endl;
    std::cout << "string: " << value->IsString() << std::endl;
    std::cout << "symbol: " << value->IsSymbol() << std::endl;
    std::cout << "function: " << value->IsFunction() << std::endl;
    std::cout << "array: " << value->IsArray() << std::endl;
    std::cout << "object: " << value->IsObject() << std::endl;
    std::cout << "boolean: " << value->IsBoolean() << std::endl;
    std::cout << "number: " << value->IsNumber() << std::endl;
    std::cout << "external: " << value->IsExternal() << std::endl;
    std::cout << "isint32: " << value->IsInt32() << std::endl;
    std::cout << "isuint32: " << value->IsUint32() << std::endl;
    std::cout << "date: " << value->IsDate() << std::endl;
    std::cout << "argument object: " << value->IsArgumentsObject() << std::endl;
    std::cout << "boolean object: " << value->IsBooleanObject() << std::endl;
    std::cout << "number object: " << value->IsNumberObject() << std::endl;
    std::cout << "string object: " << value->IsStringObject() << std::endl;
    std::cout << "symbol object: " << value->IsSymbolObject() << std::endl;
    std::cout << "native error: " << value->IsNativeError() << std::endl;
    std::cout << "regexp: " << value->IsRegExp() << std::endl;
    std::cout << "generator function: " << value->IsGeneratorFunction() << std::endl;
    std::cout << "generator object: " << value->IsGeneratorObject() << std::endl;
}


std::vector<std::string> get_object_keys(v8::Isolate * isolate,
                                                v8::Local<v8::Object> object,
                                                bool own_properties_only)
{
    auto context = isolate->GetCurrentContext();
    v8::Local<v8::Array> properties;
    if (own_properties_only) {
        properties = object->GetOwnPropertyNames(context).ToLocalChecked();
    } else {
        properties = object->GetPropertyNames(context).ToLocalChecked();
    }
    auto array_length = get_array_length(isolate, properties);

    std::vector<std::string> keys;
    keys.reserve(array_length);

    for (int i = 0; i < array_length; i++) {
        auto property_name = properties->Get(context, i).ToLocalChecked();
        std::string name;
        if (property_name->IsSymbol()) {
            name = *v8::String::Utf8Value(v8::Local<v8::Symbol>::Cast(property_name)->Name());
        } else {
            name = *v8::String::Utf8Value(property_name);
        }
        assert(!name.empty());
        keys.push_back(name);
    }

    return keys;
}

std::vector<std::string> get_object_keys(v8::Local<v8::Object> object, bool own_properties_only) {
    auto isolate = v8::Isolate::GetCurrent();
    return get_object_keys(isolate, object, own_properties_only);
}


// set to enable debug prints on calls to stringify
#define STRINGIFY_VALUE_DEBUG false

std::string stringify_value(v8::Local<v8::Value> value,
                            bool show_all_properties,
                            std::vector<v8::Local<v8::Value>> && processed_values)
{
    
    auto isolate = v8::Isolate::GetCurrent();
    if (value.IsEmpty()) {
        return "<Empty v8::Local<v8::Value>>";
    }


    auto context = isolate->GetCurrentContext();

    std::stringstream output;

    // Only protect against cycles on container types - otherwise a numeric value with
    //   the same number won't get shown twice
    if (value->IsObject() || value->IsArray()) {
        for (auto & processed_value : processed_values) {
            if (processed_value == value) {
                if (STRINGIFY_VALUE_DEBUG) print_v8_value_details(value);
                if (STRINGIFY_VALUE_DEBUG) printf("Skipping previously processed value\n");
                return "";
            }
        }
        processed_values.push_back(value);
    }


    if (value.IsEmpty()) {
        if (STRINGIFY_VALUE_DEBUG) printf("Value IsEmpty\n");
        return "Value specified as an empty v8::Local";
    }

    // if the left is a bool, return true if right is a bool and they match, otherwise false
    if (value->IsBoolean() || value->IsNumber() || value->IsUndefined() || value->IsNull()) {
        if (STRINGIFY_VALUE_DEBUG) printf("Stringify: treating value as 'normal'\n");
        v8::String::Utf8Value value_utf8value(value);
        auto string = *value_utf8value;
        output << (string ? string : "<COULD NOT CONVERT TO STRING>");
        //output << "<something broken here>";
    } else if (value->IsFunction()) {
        v8::String::Utf8Value value_utf8value(value);
        auto string = *value_utf8value;
        output << (string ? string : "FUNCTION: <COULD NOT CONVERT TO STRING>");
//
//        output << "FUNCTION BUT PRINTING THEM IS WEIRD";
//        if (value->IsObject()) {
//            output << " - and is object too";
//        }
    } else if (value->IsString()) {
        output << "\"" << *v8::String::Utf8Value(value) << "\"";
    } else if (value->IsArray()) {
        // printf("Stringify: treating value as array\n");
        auto array = v8::Local<v8::Array>::Cast(value);
        auto array_length = get_array_length(isolate, array);

        output << "[";
        auto first_element = true;
        for (int i = 0; i < array_length; i++) {
            if (!first_element) {
                output << ", ";
            }
            first_element = false;
            auto value = array->Get(context, i);
            output << stringify_value(value.ToLocalChecked(), show_all_properties, std::move(processed_values));
        }
        output << "]";
    } else {
        // printf("Stringify: treating value as object\n");
        // check this last in case it's some other type of more specialized object we will test the specialization instead (like an array)
        // objects must have all the same keys and each key must have the same as determined by calling this function on each value
        // printf("About to see if we can stringify this as an object\n");
        // print_v8_value_details(value);
        auto object = v8::Local<v8::Object>::Cast(value);
        output << "Object type: " << *v8::String::Utf8Value(object->GetConstructorName()) << std::endl;
        if (object->InternalFieldCount() > 0) {
            output << "Internal field pointer: " << (void *)v8::External::Cast(*object->GetInternalField(0))->Value() << std::endl;
        }
        if(value->IsObject() && !object.IsEmpty()) {
            if (STRINGIFY_VALUE_DEBUG) printf("Stringifying object\n");
            output << "{";
            auto keys = get_object_keys(isolate, object, !show_all_properties);
            auto first_key = true;
            for(auto key : keys) {
                if (STRINGIFY_VALUE_DEBUG) printf("Stringify: object key %s\n", key.c_str());
                if (!first_key) {
                    output << ", ";
                }
                first_key = false;
                output << key;
                output << ": ";
                auto value = object->Get(context, v8::String::NewFromUtf8(isolate, key.c_str()));
                output << stringify_value(value.ToLocalChecked(), show_all_properties, std::move(processed_values));
            }
            output << "}";
        }
    }
    return output.str();
}





v8::Local<v8::Value> call_simple_javascript_function(v8::Isolate * isolate,
						     v8::Local<v8::Function> function) {

    v8::Local<v8::Context> context(isolate->GetCurrentContext());
    v8::TryCatch tc(isolate);

    auto maybe_result = function->Call(context, context->Global(), 0, nullptr);
    if(tc.HasCaught() || maybe_result.IsEmpty()) {
        ReportException(isolate, &tc);
        throw InvalidCallException(*v8::String::Utf8Value(tc.Exception()));
    }
    return maybe_result.ToLocalChecked();
}


// copied from shell_cc example
void ReportException(v8::Isolate* isolate, v8::TryCatch* try_catch) {

    std::stringstream result;

    v8::HandleScope handle_scope(isolate);
    v8::String::Utf8Value exception(try_catch->Exception());
    const char* exception_string = *exception;
    v8::Local<v8::Message> message = try_catch->Message();
    if (message.IsEmpty()) {
        // V8 didn't provide any extra information about this error; just
        // print the exception.
        result << exception_string;
    } else {
        // Print (filename):(line number): (message).
        v8::String::Utf8Value filename(message->GetScriptOrigin().ResourceName());
        v8::Local<v8::Context> context(isolate->GetCurrentContext());
        const char* filename_string = *filename;
        int linenum = message->GetLineNumber(context).FromJust();
        result << fmt::format("{}:{}: {}", filename_string, linenum, exception_string) << std::endl;
        // Print line of source code.
        v8::String::Utf8Value sourceline(
                message->GetSourceLine(context).ToLocalChecked());
        const char* sourceline_string = *sourceline;
        result << sourceline_string << std::endl;
        // Print wavy underline (GetUnderline is deprecated).
        int start = message->GetStartColumn(context).FromJust();
        for (int i = 0; i < start; i++) {
            result << " ";
        }
        int end = message->GetEndColumn(context).FromJust();
        for (int i = start; i < end; i++) {
            result << "^";
        }
        result << std::endl;
        v8::Local<v8::Value> stack_trace_string;
        if (try_catch->StackTrace(context).ToLocal(&stack_trace_string) &&
            stack_trace_string->IsString() &&
            v8::Local<v8::String>::Cast(stack_trace_string)->Length() > 0) {
            v8::String::Utf8Value stack_trace(stack_trace_string);
            const char* stack_trace_string = *stack_trace;
            result << stack_trace_string;
        }
    }
    log.error(LoggingSubjects::Subjects::RUNTIME_EXCEPTION, result.str());
}

bool global_name_conflicts(const std::string & name) {
    if (std::find(reserved_global_names.begin(), reserved_global_names.end(), name) !=
        reserved_global_names.end()) {
        std::cerr << fmt::format("{} is a reserved js global name", name) << std::endl;
        return true;
    }
    return false;
}

static std::vector<std::string> reserved_static_names = {"arguments", "arity", "caller", "displayName",
                                                  "length", "name", "prototype"};

bool is_reserved_word_in_static_context(std::string const & name) {
    return std::find(reserved_static_names.begin(), reserved_static_names.end(), name) != reserved_static_names.end();
}


v8::Local<v8::String> make_js_string(std::string_view str) {
    auto isolate = v8::Isolate::GetCurrent();
    return v8::String::NewFromUtf8(isolate,
                                   str.data(),
                                   v8::String::NewStringType::kNormalString,
                                   str.length());
}

std::string make_cpp_string(v8::Local<v8::Value> value) {
    if (value->IsString()) {
        return *v8::String::Utf8Value(value);
    } else if (value->IsSymbol()) {
        return *v8::String::Utf8Value(v8::Local<v8::Symbol>::Cast(value)->Name());
    } else {
        return *v8::String::Utf8Value(value->ToString());
    }
}



} // end namespace v8toolkit
