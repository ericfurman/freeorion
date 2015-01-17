#include "ValueRefParserImpl.h"

namespace parse {
    struct double_complex_parser_rules {
        double_complex_parser_rules() {
            qi::_1_type _1;
            qi::_a_type _a;
            qi::_b_type _b;
            qi::_c_type _c;
            qi::_d_type _d;
            qi::_e_type _e;
            qi::_val_type _val;
            using phoenix::construct;
            using phoenix::new_;

            const parse::lexer& tok =
                parse::lexer::instance();
            //const parse::value_ref_parser_rule<int>::type& int_value_ref =
            //    parse::value_ref_parser<int>();
            const parse::value_ref_parser_rule<std::string>::type& string_value_ref =
                parse::value_ref_parser<std::string>();

            part_capacity
                =   (
                            tok.PartCapacity_ [ _a = construct<std::string>(_1) ]
                        >   parse::label(Name_token)   >>   string_value_ref [ _d = _1 ]
                    ) [ _val = new_<ValueRef::ComplexVariable<double> >(_a, _b, _c, _d, _e) ]
                ;

            start
                %=   part_capacity
                ;

            part_capacity.name("PartCapacity");

#if DEBUG_DOUBLE_COMPLEX_PARSERS
            debug(part_capacity);
#endif
        }

        complex_variable_rule<double>::type part_capacity;
        complex_variable_rule<double>::type start;
    };

    namespace detail {
        double_complex_parser_rules double_complex_parser;
    }
}

const complex_variable_rule<double>::type& double_var_complex()
{ return parse::detail::double_complex_parser.start; }