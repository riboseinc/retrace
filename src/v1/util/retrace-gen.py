import shlex
import sys

prefixes_allowed_in_types = ("struct", "enum", "const");
subfixes_allowed_in_types = ("*")


def get_function_type(tokens, pos):
    complete_type = ""
    i = pos
    if tokens[i] is ";":
        return -1, ""

    if tokens[i] is ")":
        return -1, "void"

    while i < len(tokens) and tokens[i] in prefixes_allowed_in_types:
        complete_type += tokens[i];
        complete_type += " "
        i += 1

    complete_type += tokens[i] + " ";
    i += 1

    while i < len(tokens) and tokens[i] in subfixes_allowed_in_types:
        complete_type += tokens[i];
        i += 1

    return i, complete_type.strip()

def get_function_name(tokens, pos):
    if len(tokens) > (pos + 2):
        function_name = tokens[pos]

        if tokens[pos + 1] is not "(":
            sys.exit ("Expected (")

        return pos + 2, function_name.strip()
    else:
        return -1, ""

def get_parameter_name(tokens, pos):
    if len(tokens) > (pos + 2):
        function_name = tokens[pos]

        if tokens[pos + 1] is not "," and tokens[pos + 1] is not ")":
            sys.exit ("Expected ) or ,")

        return pos + 2, function_name
    else:
        return -1, ""


def get_parameter(tokens, pos):
    parameter_name = ""
    pos, parameter_type = get_function_type(tokens, pos)

    if pos is not -1:
        pos, parameter_name = get_parameter_name(tokens, pos)

    return pos, parameter_type, parameter_name

def c_type_to_retrace_type(type):
    return {
        'char *' : 'PARAMETER_TYPE_STRING',
        'FILE *' : 'PARAMETER_TYPE_FILE_STREAM',
        'int'    : 'PARAMETER_TYPE_INT',
        'long'    : 'PARAMETER_TYPE_INT',
        'char'   : 'PARAMETER_TYPE_CHAR'
    }.get(type, 'PARAMETER_TYPE_POINTER')

parameter_list = list()
parameter_str_with_types = ""
parameter_str = ""
parameter_str_pointers = ""
parameter_str_retrace = ""

prototype = sys.argv[1]

lexer = shlex.shlex(prototype)
tokenList = []
for token in lexer:
    tokenList.append(str(token))

index, function_type = get_function_type(tokenList, 0)                     # Get function type
index, function_name = get_function_name(tokenList, index)                 # Get function name

# Get parameter list
index, parameter_type, parameter_name = get_parameter(tokenList, index)
parameter_list.append({'type' : parameter_type, 'name' : parameter_name})

if parameter_type == 'void':
    parameter_str_with_types = "void"
else:
    parameter_str_with_types = parameter_type + " " + parameter_name
    parameter_str = parameter_name;
    parameter_str_pointers = "&" + parameter_name;
    parameter_str_retrace = c_type_to_retrace_type(parameter_type)

while index is not -1:
    index, parameter_type, parameter_name = get_parameter(tokenList, index)
    if index is not -1:
        parameter_list.append({'type' : parameter_type, 'name' : parameter_name})
        parameter_str_with_types += ", " + parameter_type + " " + parameter_name
        parameter_str += ", " + parameter_name
        parameter_str_pointers += ", &" + parameter_name;
        parameter_str_retrace += ", " + c_type_to_retrace_type(parameter_type)

if parameter_str_retrace:
    parameter_str_retrace += ", "

parameter_str_retrace += "PARAMETER_TYPE_END"

data = {"parameter_str_with_types" : parameter_str_with_types,
        "parameter_str" : parameter_str,
        "parameter_str_pointers" : parameter_str_pointers,
        "parameter_str_retrace" : parameter_str_retrace,
        "function_type" : function_type,
        "function_name" : function_name,
        "function_type_retrace" : c_type_to_retrace_type(function_type)}

template = '''
typedef {function_type} (*rtr_{function_name}_t)({parameter_str_with_types});

RETRACE_DECL({function_name});

{function_type} RETRACE_IMPLEMENTATION({function_name})({parameter_str_with_types})
{{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {{{parameter_str_retrace}}};
	void *parameter_values[] = {{{parameter_str_pointers}}};
	{function_type} ret;

	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "{function_name}";
	event_info.function_group = RTR_FUNC_GRP_PROC;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = parameter_values;
	event_info.return_value_type = {function_type_retrace};
	event_info.return_value = &ret;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;
	retrace_log_and_redirect_before(&event_info);

	ret = real_{function_name}({parameter_str});

	retrace_log_and_redirect_after(&event_info);

	return ret;
}}

RETRACE_REPLACE({function_name}, {function_type}, ({parameter_str_with_types}), ({parameter_str}))

'''

print template.format (**data)
