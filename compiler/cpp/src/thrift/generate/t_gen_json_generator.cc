/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements. See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership. The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License. You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied. See the License for the
 * specific language governing permissions and limitations
 * under the License.
 *
 * Contains some contributions under the Thrift Software License.
 * Please see doc/old-thrift-license.txt in the Thrift distribution for
 * details.
 */

#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>

#include <sstream>
#include <stdlib.h>
#include <sys/stat.h>

#include "thrift/generate/t_generator.h"
#include "thrift/platform.h"
#include <boost/algorithm/string.hpp> // by joygram 2020/03/07


using std::map;
using std::ofstream;
using std::ostream;
using std::ostringstream;
using std::stack;
using std::string;
using std::stringstream;
using std::vector;

static const string endl = "\n";
static const string quot = "\"";
static const bool NO_INDENT = false;
static const bool FORCE_STRING = true;

class t_gen_json_generator : public t_generator {
public:
	t_gen_json_generator(t_program* program,
		const std::map<std::string, std::string>& parsed_options,
		const std::string& option_string)
		: t_generator(program) {
		(void)option_string;
		std::map<std::string, std::string>::const_iterator iter;

		should_merge_includes_ = false;
		for (iter = parsed_options.begin(); iter != parsed_options.end(); ++iter) {
			if (iter->first.compare("merge") == 0) {
				should_merge_includes_ = true;
			}
			else {
				throw "unknown option json:" + iter->first;
			}
		}

		out_dir_base_ = "gen-json";
	}

	~t_gen_json_generator() override = default;

	/**
	 * Init and close methods
	 */

	void init_generator() override;
	void close_generator() override;

	void generate_typedef(t_typedef* ttypedef) override;
	void generate_enum(t_enum* tenum) override;
	void generate_program() override;
	void generate_function(t_function* tfunc);
	void generate_field(t_field* field, int32_t order = 0);

	void generate_service(t_service* tservice) override;
	void generate_struct(t_struct* tstruct) override;

	void generate_typedef_gen(t_typedef* ttypedef);
	void generate_enum_gen(t_enum* tenum);
	void generate_enum_doc_gen(t_enum* tenum); // enums_doc by joygram 2020/03/07

	void generate_struct_gen(t_struct* tstruct);

private:
	bool should_merge_includes_;

	ofstream_with_content_based_conditional_update f_json_;
	std::stack<bool> comma_needed_;

	template <typename T>
	string number_to_string(T t) {
		std::ostringstream out;
		out.imbue(std::locale::classic());
		out.precision(std::numeric_limits<T>::digits10);
		out << t;
		return out.str();
	}

	template <typename T>
	void write_number(T n) {
		f_json_ << number_to_string(n);
	}

	string get_type_name_(
		t_type* ttype); // old get_type_name : rename to get_type_name -> get_type_name_ by joygram
	string get_type_name(t_type* ttype);
	string get_qualified_name(t_type* ttype);

	string get_datatype_name(t_type* type); // joygram 2015/06/24
	string get_typedef_name(t_type* type);  // joygram 2015/06/24

	void start_object(bool should_indent = true);
	void start_array();
	void end_object(bool new_line = false); // new_line add by joygram
	void end_array();
	void write_comma_if_needed();
	void indicate_comma_needed();
	string escape_json_string(const string& input);
	string json_str(const string& str);
	void merge_includes(t_program*);

	void generate_constant(t_const* con);

	void write_type_spec_entry(const char* name, t_type* ttype);
	void write_type_spec_object(const char* name, t_type* ttype);
	void write_type_spec(t_type* ttype);
	void write_string(const string& value);
	// void write_value(t_type* tvalue);
	void write_const_value(t_const_value* value, bool force_string = false);
	void write_key_and(string key);
	void write_key_and_string(string key, string val);
	void write_key_and_integer(string key, int val);
	void write_key_and_bool(string key, bool val);

	void write_key_only(string key);                   // for enum name by joygram
	void write_value_integer(int val);                 // for add index by joygram
	void write_integer_and_key(int key, string value); // for add index based enum by joygram
};

void t_gen_json_generator::init_generator() {
	MKDIR(get_out_dir().c_str());

	string f_json_name = get_out_dir() + program_->get_name() + ".json";
	f_json_.open(f_json_name.c_str());

	// Merge all included programs into this one so we can output one big file.
	if (should_merge_includes_) {
		merge_includes(program_);
	}
}

string t_gen_json_generator::escape_json_string(const string& input) {
	std::ostringstream ss;
	for (char iter : input) {
		switch (iter) {
		case '\\':
			ss << "\\\\";
			break;
		case '"':
			ss << "\\\"";
			break;
		case '/':
			ss << "\\/";
			break;
		case '\b':
			ss << "\\b";
			break;
		case '\f':
			ss << "\\f";
			break;
		case '\n':
			ss << "\\n";
			break;
		case '\r':
			ss << "\\r";
			break;
		case '\t':
			ss << "\\t";
			break;
		default:
			ss << iter;
			break;
		}
	}
	return ss.str();
}

void t_gen_json_generator::start_object(bool should_indent) {
	f_json_ << (should_indent ? indent() : "") << "{" << endl;
	indent_up();
	comma_needed_.push(false);
}

void t_gen_json_generator::start_array() {
	f_json_ << "[" << endl;
	indent_up();
	comma_needed_.push(false);
}

void t_gen_json_generator::write_comma_if_needed() {
	if (comma_needed_.top()) {
		f_json_ << "," << endl;
	}
}

void t_gen_json_generator::indicate_comma_needed() {
	comma_needed_.pop();
	comma_needed_.push(true);
}

void t_gen_json_generator::write_key_and(string key) {
	write_comma_if_needed();
	indent(f_json_) << json_str(key) << ": ";
	indicate_comma_needed();
}

void t_gen_json_generator::write_key_and_integer(string key, int val) {
	write_comma_if_needed();
	indent(f_json_) << json_str(key) << ": " << number_to_string(val);
	indicate_comma_needed();
}

void t_gen_json_generator::write_key_and_string(string key, string val) {
	write_comma_if_needed();
	indent(f_json_) << json_str(key) << ": " << json_str(val);
	indicate_comma_needed();
}

void t_gen_json_generator::write_key_and_bool(string key, bool val) {
	write_comma_if_needed();
	indent(f_json_) << json_str(key) << ": " << (val ? "true" : "false");
	indicate_comma_needed();
}

void t_gen_json_generator::write_value_integer(int val) // by joygram
{
	f_json_ << quot << val << quot << ":";
}

void t_gen_json_generator::write_key_only(string key) // by joygram
{
	f_json_ << quot << key << quot << ":";
}

void t_gen_json_generator::write_integer_and_key(int key, string val) // by joygram
{
	write_comma_if_needed();
	indent(f_json_) << number_to_string(key) << ": " << json_str(val);
	indicate_comma_needed();
}
// add new line by joygram
void t_gen_json_generator::end_object(bool new_line) {
	indent_down();
	f_json_ << endl << indent() << "}";
	if (new_line) {
		f_json_ << endl;
	}
	comma_needed_.pop();
}

void t_gen_json_generator::end_array() {
	indent_down();
	if (comma_needed_.top()) {
		f_json_ << endl;
	}
	indent(f_json_) << "]";
	comma_needed_.pop();
}

void t_gen_json_generator::write_type_spec_object(const char* name, t_type* ttype) {
	ttype = ttype->get_true_type();
	if (ttype->is_struct() || ttype->is_xception() || ttype->is_container()) {
		write_key_and(name);
		start_object(NO_INDENT);
		write_key_and("typeId");
		write_type_spec(ttype);
		end_object();
	}
}

void t_gen_json_generator::write_type_spec_entry(const char* name, t_type* ttype) {
	write_key_and(name);
	write_type_spec(ttype);
}

void t_gen_json_generator::write_type_spec(t_type* ttype) {
	ttype = ttype->get_true_type();

	write_string(get_type_name(ttype));

	if (ttype->annotations_.size() > 0) {
		write_key_and("annotations");
		start_object();
		for (auto& annotation : ttype->annotations_) {
			write_key_and_string(annotation.first, annotation.second);
		}
		end_object();
	}

	if (ttype->is_struct() || ttype->is_xception()) {
		write_key_and_string("class", get_qualified_name(ttype));
	}
	else if (ttype->is_map()) {
		t_type* ktype = ((t_map*)ttype)->get_key_type();
		t_type* vtype = ((t_map*)ttype)->get_val_type();
		write_key_and_string("keyTypeId", get_type_name(ktype));
		write_key_and_string("valueTypeId", get_type_name(vtype));
		write_type_spec_object("keyType", ktype);
		write_type_spec_object("valueType", vtype);
	}
	else if (ttype->is_list()) {
		t_type* etype = ((t_list*)ttype)->get_elem_type();
		write_key_and_string("elemTypeId", get_type_name(etype));
		write_type_spec_object("elemType", etype);
	}
	else if (ttype->is_set()) {
		t_type* etype = ((t_set*)ttype)->get_elem_type();
		write_key_and_string("elemTypeId", get_type_name(etype));
		write_type_spec_object("elemType", etype);
	}
}

void t_gen_json_generator::close_generator() {
	f_json_ << endl;
	f_json_.close();
}

void t_gen_json_generator::merge_includes(t_program* program) {
	vector<t_program*> includes = program->get_includes();
	vector<t_program*>::iterator inc_iter;
	for (inc_iter = includes.begin(); inc_iter != includes.end(); ++inc_iter) {
		t_program* include = *inc_iter;
		// recurse in case we get crazy
		merge_includes(include);
		// merge enums
		vector<t_enum*> enums = include->get_enums();
		vector<t_enum*>::iterator en_iter;
		for (en_iter = enums.begin(); en_iter != enums.end(); ++en_iter) {
			program->add_enum(*en_iter);
		}
		// merge typedefs
		vector<t_typedef*> typedefs = include->get_typedefs();
		vector<t_typedef*>::iterator td_iter;
		for (td_iter = typedefs.begin(); td_iter != typedefs.end(); ++td_iter) {
			program->add_typedef(*td_iter);
		}
		// merge structs
		vector<t_struct*> objects = include->get_objects();
		vector<t_struct*>::iterator o_iter;
		for (o_iter = objects.begin(); o_iter != objects.end(); ++o_iter) {
			program->add_struct(*o_iter);
		}
		// merge constants
		vector<t_const*> consts = include->get_consts();
		vector<t_const*>::iterator c_iter;
		for (c_iter = consts.begin(); c_iter != consts.end(); ++c_iter) {
			program->add_const(*c_iter);
		}

		// merge services
		vector<t_service*> services = include->get_services();
		vector<t_service*>::iterator sv_iter;
		for (sv_iter = services.begin(); sv_iter != services.end(); ++sv_iter) {
			program->add_service(*sv_iter);
		}
	}
}

void t_gen_json_generator::generate_program() {

	init_generator();

	start_object();
	write_key_and_string("name", program_->get_name());
	std::string doc_text; //doc필드 항상 출력 by joygram 2020/03/07
	if (program_->has_doc()) {
		doc_text = boost::algorithm::trim_copy(program_->get_doc()); // trim doc by joygram 2020/03/07
	}
	write_key_and_string("doc", doc_text);

	// When merging includes, the "namespaces" and "includes" sections
	// become ambiguous, so just skip them.
	if (!should_merge_includes_) {
		// Generate namespaces
		write_key_and("namespaces");
		start_object(NO_INDENT);
		const map<string, string>& namespaces = program_->get_namespaces();
		map<string, string>::const_iterator ns_it;
		for (ns_it = namespaces.begin(); ns_it != namespaces.end(); ++ns_it) {
			write_key_and_string(ns_it->first, ns_it->second);
			indicate_comma_needed();
		}
		end_object();

		// Generate includes
		write_key_and("includes");
		start_array();
		const vector<t_program*> includes = program_->get_includes();
		vector<t_program*>::const_iterator inc_it;
		for (inc_it = includes.begin(); inc_it != includes.end(); ++inc_it) {
			write_comma_if_needed();
			write_string((*inc_it)->get_name());
			indicate_comma_needed();
		}
		end_array();
	}

	// Generate enums
	write_key_and("enums");
	// start_array();
	start_object(); // enum for gen
	vector<t_enum*> enums = program_->get_enums();
	vector<t_enum*>::iterator en_iter;
	for (en_iter = enums.begin(); en_iter != enums.end(); ++en_iter) {
		write_comma_if_needed();
		generate_enum_gen(*en_iter); // enum for gen
		indicate_comma_needed();
	}
	end_object(); // enum for gen
	// end_array();

	// generate enums_doc by joygram 2020/03/07
	write_key_and("enums_doc");
	// start_array();
	start_object(); // enum for gen
	vector<t_enum*> enums_doc = program_->get_enums();
	vector<t_enum*>::iterator edoc_iter;
	for (edoc_iter = enums_doc.begin(); edoc_iter != enums_doc.end(); ++edoc_iter) {
		write_comma_if_needed();
		generate_enum_doc_gen(*edoc_iter); // enum for gen
		indicate_comma_needed();
	}
	end_object(); // enum for gen

	// Generate typedefs
	write_key_and("typedefs");
	// start_array();
	start_object(); // typedefs for gen
	vector<t_typedef*> typedefs = program_->get_typedefs();
	vector<t_typedef*>::iterator td_iter;

	//중복제거 by joygram 2020/03/07
	std::map<std::string, t_typedef*> typedef_hash;
	for (td_iter = typedefs.begin(); td_iter != typedefs.end(); ++td_iter) {
		t_typedef* ttypedef = *td_iter;
		string ns = "__" + ttypedef->get_program()->get_namespace("cpp") + "__";
		string typedef_name = ns + get_qualified_name(*td_iter);
		typedef_hash.insert(std::pair< std::string, t_typedef*>(typedef_name, ttypedef));
	}
	auto it = typedef_hash.begin();
	while (it != typedef_hash.end())
	{
		write_comma_if_needed();
		generate_typedef_gen(it->second); // typedef for gen
		indicate_comma_needed();
		++it;
	}

	//for (td_iter = typedefs.begin(); td_iter != typedefs.end(); ++td_iter) {
	//	write_comma_if_needed();
	//	generate_typedef_gen(*td_iter); // typedef for gen
	//	indicate_comma_needed();
	//}
	end_object(); // typedefs for gen
	// end_array();

	// Generate structs, exceptions, and unions in declared order
	write_key_and("structs");
	// start_array();
	start_object(); // structs for gen
	vector<t_struct*> objects = program_->get_objects();
	vector<t_struct*>::iterator o_iter;
	for (o_iter = objects.begin(); o_iter != objects.end(); ++o_iter) {
		write_comma_if_needed();
		if ((*o_iter)->is_xception()) {
			generate_xception(*o_iter);
		}
		else {
			generate_struct_gen(*o_iter);
		}
		indicate_comma_needed();
	}
	end_object(); // structs for gen
	// end_array();

	// Generate constants
	write_key_and("constants");
	start_array();
	vector<t_const*> consts = program_->get_consts();
	vector<t_const*>::iterator c_iter;
	for (c_iter = consts.begin(); c_iter != consts.end(); ++c_iter) {
		write_comma_if_needed();
		generate_constant(*c_iter);
		indicate_comma_needed();
	}
	end_array();

	// Generate services
	write_key_and("services");
	start_array();
	vector<t_service*> services = program_->get_services();
	vector<t_service*>::iterator sv_iter;
	for (sv_iter = services.begin(); sv_iter != services.end(); ++sv_iter) {
		write_comma_if_needed();
		generate_service(*sv_iter);
		indicate_comma_needed();
	}
	end_array();

	end_object();

	// Close the generator
	close_generator();
}

void t_gen_json_generator::generate_typedef(t_typedef* ttypedef) {
	start_object();
	write_key_and_string("name", get_qualified_name(ttypedef));
	write_key_and_string("typeId", get_type_name(ttypedef->get_true_type()));
	write_type_spec_object("type", ttypedef->get_true_type());
	std::string doc_text;
	if (ttypedef->has_doc()) {
		doc_text = boost::algorithm::trim_copy(ttypedef->get_doc()); // trim doc by joygram 2020/03/07
	}
	write_key_and_string("doc", doc_text);

	if (ttypedef->annotations_.size() > 0) {
		write_key_and("annotations");
		start_object();
		for (map<string, string>::iterator it = ttypedef->annotations_.begin();
			it != ttypedef->annotations_.end(); ++it) {
			write_key_and_string(it->first, it->second);
		}
		end_object();
	}
	end_object();
}

void t_gen_json_generator::generate_typedef_gen(t_typedef* ttypedef) {

	string ns = "__" + ttypedef->get_program()->get_namespace("cpp") + "__";
	string typedef_name = ns + get_qualified_name(ttypedef);
	write_key_only(typedef_name);

	// typedef_name이 중복이면 skip

	start_object();
	// write_key_and_string("name", get_qualified_name(ttypedef));
	write_key_and_string("typeId", get_type_name(ttypedef->get_true_type()));
	write_type_spec_object("type", ttypedef->get_true_type());

	std::string doc_text; //doc필드 항상 출력 by joygram 2020/03/07
	if (ttypedef->has_doc()) {
		doc_text = boost::algorithm::trim_copy(ttypedef->get_doc()); // trim doc by joygram 2020/03/07
	}
	write_key_and_string("doc", doc_text);
	//if (ttypedef->has_doc()) {
	//	write_key_and_string("doc", ttypedef->get_doc());
	//}

	if (ttypedef->annotations_.size() > 0) {
		write_key_and("annotations");
		start_object();
		for (map<string, string>::iterator it = ttypedef->annotations_.begin();
			it != ttypedef->annotations_.end(); ++it) {
			write_key_and_string(it->first, it->second);
		}
		end_object();
	}
	end_object();
}

void t_gen_json_generator::write_string(const string& value) {
	f_json_ << quot << escape_json_string(value) << quot;
}

void t_gen_json_generator::write_const_value(t_const_value* value, bool should_force_string) {

	switch (value->get_type()) {

	case t_const_value::CV_IDENTIFIER:
	case t_const_value::CV_INTEGER:
		if (should_force_string) {
			write_string(number_to_string(value->get_integer()));
		}
		else {
			write_number(value->get_integer());
		}
		break;

	case t_const_value::CV_DOUBLE:
		if (should_force_string) {
			write_string(number_to_string(value->get_double()));
		}
		else {
			write_number(value->get_double());
		}
		break;

	case t_const_value::CV_STRING:
		write_string(value->get_string());
		break;

	case t_const_value::CV_LIST: {
		start_array();
		std::vector<t_const_value*> list = value->get_list();
		std::vector<t_const_value*>::iterator lit;
		for (lit = list.begin(); lit != list.end(); ++lit) {
			write_comma_if_needed();
			f_json_ << indent();
			write_const_value(*lit);
			indicate_comma_needed();
		}
		end_array();
		break;
	}

	case t_const_value::CV_MAP: {
		start_object(NO_INDENT);
		std::map<t_const_value*, t_const_value*, t_const_value::value_compare> map = value->get_map();
		std::map<t_const_value*, t_const_value*, t_const_value::value_compare>::iterator mit;
		for (mit = map.begin(); mit != map.end(); ++mit) {
			write_comma_if_needed();
			f_json_ << indent();
			// JSON objects only allow string keys
			write_const_value(mit->first, FORCE_STRING);
			f_json_ << ": ";
			write_const_value(mit->second);
			indicate_comma_needed();
		}
		end_object();
		break;
	}

	default:
		f_json_ << "null";
		break;
	}
}

string t_gen_json_generator::json_str(const string& str) {
	return quot + escape_json_string(str) + quot;
}

void t_gen_json_generator::generate_constant(t_const* con) {
	start_object();

	// add namespace to const name by joygram by 2018/01/19
	string ns = "__" + con->namespace_ + "__";
	string const_name = ns + con->get_name();
	write_key_and_string("name", const_name);

	// write_key_and_string("name", con->get_name());
	write_key_and_string("typeId", get_type_name(con->get_type()));
	write_type_spec_object("type", con->get_type());

	std::string doc_text; //doc필드 항상 출력 by joygram 2020/03/07
	if (con->has_doc()) {
		doc_text = boost::algorithm::trim_copy(con->get_doc()); // trim doc by joygram 2020/03/07
	}
	write_key_and_string("doc", doc_text);
	//if (con->has_doc()) {
	//	write_key_and_string("doc", con->get_doc());
	//}

	write_key_and("value");
	write_const_value(con->get_value());

	end_object();
}

void t_gen_json_generator::generate_enum(t_enum* tenum) {
	start_object();

	write_key_and_string("name", tenum->get_name());
	std::string doc_text; //doc필드 항상 출력 by joygram 2020/03/07
	if (tenum->has_doc()) {
		doc_text = boost::algorithm::trim_copy(tenum->get_doc()); // trim doc by joygram 2020/03/07
	}
	write_key_and_string("doc", doc_text);
	//if (tenum->has_doc()) {
	//	write_key_and_string("doc", tenum->get_doc());
	//}

	if (tenum->annotations_.size() > 0) {
		write_key_and("annotations");
		start_object();
		for (auto& annotation : tenum->annotations_) {
			write_key_and_string(annotation.first, annotation.second);
		}
		end_object();
	}

	write_key_and("members");
	start_array();
	vector<t_enum_value*> values = tenum->get_constants();
	vector<t_enum_value*>::iterator val_iter;
	for (val_iter = values.begin(); val_iter != values.end(); ++val_iter) {
		write_comma_if_needed();
		t_enum_value* val = (*val_iter);
		start_object();
		write_key_and_string("name", val->get_name());
		write_key_and_integer("value", val->get_value());
		std::string doc_text; //doc필드 항상 출력 by joygram 2020/03/07
		if (val->has_doc()) {
			doc_text = boost::algorithm::trim_copy(val->get_doc()); // trim doc by joygram 2020/03/07
		}
		write_key_and_string("doc", doc_text);
		//if (val->has_doc()) {
		//	write_key_and_string("doc", val->get_doc());
		//}
		end_object();
		indicate_comma_needed();
	}
	end_array();

	end_object();
}
/*
enum style change by joygram
"ns.enumname" :{
"NAME1" : "1",
"NAME2" : "2",
}
*/
void t_gen_json_generator::generate_enum_gen(t_enum* tenum) {

	string ns = "__" + tenum->get_program()->get_namespace("cpp") + "__";
	string enum_name = ns + tenum->get_name();
	write_key_only(enum_name);

	start_object();
	vector<t_enum_value*> values = tenum->get_constants();
	vector<t_enum_value*>::iterator val_iter;
	for (val_iter = values.begin(); val_iter != values.end(); ++val_iter) {
		t_enum_value* val = (*val_iter);
		write_key_and_string(val->get_name(), number_to_string(val->get_value())); // key : value로 변경
		// write_key_and_string(number_to_string(val->get_value()), val->get_name());
	}
	end_object();
}

#include <boost/algorithm/string.hpp>
void t_gen_json_generator::generate_enum_doc_gen(t_enum* tenum) {

	string ns = "__" + tenum->get_program()->get_namespace("cpp") + "__";
	string enum_name = ns + tenum->get_name();
	write_key_only(enum_name);

	start_object();
	vector<t_enum_value*> values = tenum->get_constants();
	vector<t_enum_value*>::iterator val_iter;
	for (val_iter = values.begin(); val_iter != values.end(); ++val_iter) {
		t_enum_value* val = (*val_iter);
		std::string doc_text;
		if (val->has_doc()) {
			doc_text = boost::algorithm::trim_copy(val->get_doc());
		}
		write_key_and_string(val->get_name(), doc_text);

	}
	end_object();
}

void t_gen_json_generator::generate_struct(t_struct* tstruct) {
	start_object();

	write_key_and_string("name", tstruct->get_name());

	std::string doc_text; //doc필드 항상 출력 by joygram 2020/03/07
	if (tstruct->has_doc()) {
		doc_text = boost::algorithm::trim_copy(tstruct->get_doc()); // trim doc by joygram 2020/03/07
	}
	write_key_and_string("doc", doc_text);

	//if (tstruct->has_doc()) {
	//	write_key_and_string("doc", tstruct->get_doc());
	//}

	if (tstruct->annotations_.size() > 0) {
		write_key_and("annotations");
		start_object();
		for (auto& annotation : tstruct->annotations_) {
			write_key_and_string(annotation.first, annotation.second);
		}
		end_object();
	}

	write_key_and_bool("isException", tstruct->is_xception());

	write_key_and_bool("isUnion", tstruct->is_union());

	write_key_and("fields");
	start_array();
	vector<t_field*> members = tstruct->get_members();
	vector<t_field*>::iterator mem_iter;
	for (mem_iter = members.begin(); mem_iter != members.end(); mem_iter++) {
		write_comma_if_needed();
		generate_field(*mem_iter);
		indicate_comma_needed();
	}
	end_array();

	end_object();
}

// add type name to json by joygram
void t_gen_json_generator::generate_struct_gen(t_struct* tstruct) {
	string ns = "__" + tstruct->get_program()->get_namespace("cpp") + "__";
	write_key_only(ns + tstruct->get_name());

	start_object();
	vector<t_field*> members = tstruct->get_members();
	vector<t_field*>::iterator mem_iter = members.begin();

	auto order_number = 0;
	for (; mem_iter != members.end(); mem_iter++) {
		write_comma_if_needed();
		generate_field((*mem_iter), ++order_number);
		indicate_comma_needed();
	}
	end_object(true);
}

void t_gen_json_generator::generate_service(t_service* tservice) {
	start_object();

	write_key_and_string("name", get_qualified_name(tservice));

	if (tservice->get_extends()) {
		write_key_and_string("extends", get_qualified_name(tservice->get_extends()));
	}
	std::string doc_text; //doc필드 항상 출력 by joygram 2020/03/07
	if (tservice->has_doc()) {
		doc_text = boost::algorithm::trim_copy(tservice->get_doc()); // trim doc by joygram 2020/03/07
	}
	write_key_and_string("doc", doc_text);
	//if (tservice->has_doc()) {
	//	write_key_and_string("doc", tservice->get_doc());
	//}

	if (tservice->annotations_.size() > 0) {
		write_key_and("annotations");
		start_object();
		for (auto& annotation : tservice->annotations_) {
			write_key_and_string(annotation.first, annotation.second);
		}
		end_object();
	}

	write_key_and("functions");
	start_array();
	vector<t_function*> functions = tservice->get_functions();
	vector<t_function*>::iterator fn_iter = functions.begin();
	for (; fn_iter != functions.end(); fn_iter++) {
		write_comma_if_needed();
		generate_function(*fn_iter);
		indicate_comma_needed();
	}
	end_array();

	end_object();
}

void t_gen_json_generator::generate_function(t_function* tfunc) {
	start_object();

	write_key_and_string("name", tfunc->get_name());

	write_key_and_string("returnTypeId", get_type_name(tfunc->get_returntype()));
	write_type_spec_object("returnType", tfunc->get_returntype());

	write_key_and_bool("oneway", tfunc->is_oneway());

	std::string doc_text; //doc필드 항상 출력 by joygram 2020/03/07
	if (tfunc->has_doc()) {
		doc_text = boost::algorithm::trim_copy(tfunc->get_doc()); // trim doc by joygram 2020/03/07
	}
	write_key_and_string("doc", doc_text);
	//if (tfunc->has_doc()) {
	//	write_key_and_string("doc", tfunc->get_doc());
	//}

	if (tfunc->annotations_.size() > 0) {
		write_key_and("annotations");
		start_object();
		for (auto& annotation : tfunc->annotations_) {
			write_key_and_string(annotation.first, annotation.second);
		}
		end_object();
	}

	write_key_and("arguments");
	start_array();
	vector<t_field*> members = tfunc->get_arglist()->get_members();
	vector<t_field*>::iterator mem_iter = members.begin();
	for (; mem_iter != members.end(); mem_iter++) {
		write_comma_if_needed();
		generate_field(*mem_iter);
		indicate_comma_needed();
	}
	end_array();

	write_key_and("exceptions");
	start_array();
	vector<t_field*> excepts = tfunc->get_xceptions()->get_members();
	vector<t_field*>::iterator ex_iter = excepts.begin();
	for (; ex_iter != excepts.end(); ex_iter++) {
		write_comma_if_needed();
		generate_field(*ex_iter);
		indicate_comma_needed();
	}
	end_array();

	end_object();
}

// add order by joygram
void t_gen_json_generator::generate_field(t_field* field, int32_t order) {

	write_key_only(number_to_string(field->get_key()));

	start_object();

	write_key_and_integer("key", field->get_key());
	write_key_and_string("name", field->get_name());
	write_key_and_string("typeId", get_type_name(field->get_type()));
	write_type_spec_object("type", field->get_type());

	write_key_and_string("datatype", get_datatype_name(field->get_type())); // by joygram 2015/06/24
	write_key_and_string("typedef", get_typedef_name(field->get_type()));   // by joygram 2015/06/24
	// add thrift index (key) by joygram 2016/03/21;
	{
		std::stringstream out;
		out << field->get_key();
		write_key_and_string("thrift_index", out.str());
	}
	// field order number by joygram 2016/03/21
	{
		std::stringstream out;
		out << order;
		write_key_and_string("order", out.str());
	}

	std::string doc_text; //doc필드 항상 출력 by joygram 2020/03/07
	if (field->has_doc()) {
		doc_text = boost::algorithm::trim_copy(field->get_doc()); // trim doc by joygram 2020/03/07
	}
	write_key_and_string("doc", doc_text);
	//if (field->has_doc()) {
	//	auto doc_text = boost::algorithm::trim_copy(field->get_doc()); // trim doc by joygram 2020/03/07
	//	write_key_and_string("doc", doc_text);
	//}

	if (field->annotations_.size() > 0) {
		write_key_and("annotations");
		start_object();
		for (auto& annotation : field->annotations_) {
			write_key_and_string(annotation.first, annotation.second);
		}
		end_object();
	}

	write_key_and("required");
	switch (field->get_req()) {
	case t_field::T_REQUIRED:
		write_string("required");
		break;
	case t_field::T_OPT_IN_REQ_OUT:
		write_string("req_out");
		break;
	default:
		write_string("optional");
		break;
	}

	if (field->get_value()) {
		write_key_and("default");
		write_const_value(field->get_value());
	}

	end_object();
}

string t_gen_json_generator::get_type_name_(t_type* ttype) {
	ttype = ttype->get_true_type();
	if (ttype->is_list()) {
		return "list";
	}
	if (ttype->is_set()) {
		return "set";
	}
	if (ttype->is_map()) {
		return "map";
	}
	if (ttype->is_enum()) {
		return "i32";
	}
	if (ttype->is_struct()) {
		return ((t_struct*)ttype)->is_union() ? "union" : "struct";
	}
	if (ttype->is_xception()) {
		return "exception";
	}
	if (ttype->is_base_type()) {
		t_base_type* tbasetype = (t_base_type*)ttype;
		return tbasetype->is_binary() ? "binary" : t_base_type::t_base_name(tbasetype->get_base());
	}

	return "(unknown)";
}

// detailed typename by joygram
string t_gen_json_generator::get_type_name(t_type* ttype) {
	if (ttype->is_container()) {
		if (ttype->is_list()) {
			return "list<" + get_type_name(((t_list*)ttype)->get_elem_type()) + ">";
		}
		else if (ttype->is_set()) {
			return "set<" + get_type_name(((t_set*)ttype)->get_elem_type()) + ">";
		}
		else if (ttype->is_map()) {
			return "map<" + get_type_name(((t_map*)ttype)->get_key_type()) + +","
				+ get_type_name(((t_map*)ttype)->get_val_type()) + ">";
		}
	}
	else if (ttype->is_base_type()) {
		return (((t_base_type*)ttype)->is_binary() ? "binary" : ttype->get_name());
	}

	// add type name to json by joygram
	string ns = "__" + ttype->get_program()->get_namespace("cpp") + "__";
	return ns + ttype->get_name();
}

// added by joygram 2015/06/24
string t_gen_json_generator::get_datatype_name(t_type* ttype) {
	if (ttype->is_container()) {
		if (ttype->is_list()) {
			return "lst";
		}
		else if (ttype->is_set()) {
			return "set";
		}
		else if (ttype->is_map()) {
			return "map";
		}
	}
	else if (ttype->is_enum()) {
		return "enum";
	}
	else if (ttype->is_string()) {
		return "str";
	}
	else if (ttype->is_struct()) {
		return "rec";
	}
	else if (ttype->is_bool()) {
		return "tf";
	}
	else if (ttype->is_typedef()) {
		t_type* true_type = ttype->get_true_type();
		return get_datatype_name(true_type);
	}
	else if ("double" == ttype->get_name()) // double, bool data type add by joygram 2017/07/11
	{
		return "dbl";
	}
	return ttype->get_name();
}

string t_gen_json_generator::get_typedef_name(t_type* ttype) {
	if (ttype->is_container()) {
		if (ttype->is_list()) {
			return "lst";
		}
		else if (ttype->is_set()) {
			return "set";
		}
		else if (ttype->is_map()) {
			return "map";
		}
	}
	else if (ttype->is_enum()) {
		return "enum";
	}
	else if (ttype->is_string()) {
		return "str";
	}
	else if (ttype->is_struct()) {
		return "rec";
	}
	else if (ttype->is_typedef()) {
		return ttype->get_name();
	}
	return ttype->get_name();
}

string t_gen_json_generator::get_qualified_name(t_type* ttype) {
	if (should_merge_includes_ || ttype->get_program() == program_) {
		return ttype->get_name();
	}
	return ttype->get_program()->get_name() + "." + ttype->get_name();
}

THRIFT_REGISTER_GENERATOR(gen_json,
	"GEN JSON for GameENgine Meta Schema",
	"    merge:           Generate output with included files merged\n\n")
