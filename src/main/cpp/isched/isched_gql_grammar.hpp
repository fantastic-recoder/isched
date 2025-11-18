//
// Created by groby on 2025-11-03.
//

#ifndef ISCHED_ISCHED_GQL_GRAMMAR_HPP
#define ISCHED_ISCHED_GQL_GRAMMAR_HPP

#include <tuple>
#include <tao/pegtl.hpp>
#include <tao/pegtl/contrib/parse_tree_to_dot.hpp>
#include <tao/pegtl/contrib/trace.hpp>
#include <spdlog/spdlog.h>
#include <iostream>
#include <memory>
#include <string>
#include <regex>
#include <algorithm>

namespace isched::v0_0_1 {
    namespace pegtl = tao::pegtl;
    using namespace pegtl;
    using pegtl::one;
    using pegtl::seq;
    using pegtl::star;
    using pegtl::ranges;
    using pegtl::sor;
    using pegtl::must_if;
    using pegtl::opt;
    using pegtl::at;
    using pegtl::plus;
    using parse_tree::node;

    using std::endl;
    using std::cout;
    using std::cerr;
    using std::unique_ptr;

    struct Whitespace : one<' ', '\t'> {};

    struct LineTerminator : one<'\n', '\r'> {};

    struct Ws : sor<LineTerminator,Whitespace> {};

    struct Comment : seq< one<'#'>, until<eolf>> {};

    struct Comma : one<','> {};

    //!	$	&	(	)	...	:	=	@	[	]	{	|	}
    struct Punctuator : one<'!', '$', '&', '(', ')', '*', '+', ',', '-', '.', '/', ';', '<', '=', '>', '?', '@', '[', ']', ':', '.'> {};

    struct Letter : ranges<'A', 'Z', 'a', 'z'> {};

    struct Digit : ranges<'0', '9'> {};

    // NonZeroDigit helper for numeric grammars
    struct NonZeroDigit : ranges<'1','9'> {};

    // ===== GraphQL Value Literals: IntValue (per GraphQL.html) =====
    // IntValue := '-'? ( '0' | NonZeroDigit Digit* ) not followed by '.' or exponent marker
    struct IntValueCore : sor<
        one<'0'>,
        seq< NonZeroDigit, star<Digit>>
    > {};

    struct IntValue : seq< opt< one<'-'> >, IntValueCore, not_at< sor< one<'.'>, one<'e','E'>, Letter, Digit > > > {};

    // ===== GraphQL Value Literals: FloatValue (per GraphQL.html) =====
    // FloatValue := '-'? IntegerPart ( FractionalPart ExponentPart? | ExponentPart )
    // IntegerPart := '0' | NonZeroDigit Digit*
    // FractionalPart := '.' Digit+
    // ExponentPart := ('e' | 'E') ('+' | '-')? Digit+
    // Additionally: the token must not be immediately followed by Letter or Digit
    struct FractionalPart : seq< one<'.'>, plus<Digit> > {};
    struct ExponentPart   : seq< one<'e','E'>, opt< one<'+','-'> >, plus<Digit> > {};

    struct FloatValueCore : sor<
        // IntegerPart '.' Digits [Exponent]?
        seq< IntValueCore, FractionalPart, opt<ExponentPart> >,
        // IntegerPart Exponent
        seq< IntValueCore, ExponentPart >
    > {};

    struct FloatValue : seq< opt< one<'-'> >, FloatValueCore, not_at< sor< Letter, Digit, one<'.'> > > > {};

    // ===== GraphQL Value Literals: StringValue (quoted and block strings per GraphQL spec) =====
    // Quoted (standard) string:
    //   '"' ( StringCharacter | EscapeSequence )* '"'
    // EscapeSequence: '\\' ( '"' | '\\' | '/' | 'b' | 'f' | 'n' | 'r' | 't' | 'u' HexDigit HexDigit HexDigit HexDigit )
    // StringCharacter: any code point except '"', '\\', or line terminators
    // Block string (triple-quoted):
    //   '"""' ( BlockStringChunk | EscapedTripleQuotes )* '"""'
    // Where BlockStringChunk is any single code unit that does not begin an unescaped closing delimiter (""")
    // and EscapedTripleQuotes is '\"""' to allow a literal sequence of three quotes inside.
    // Note: We intentionally do not normalize indentation/whitespace here; that is a semantic step.
    struct HexDigit : sor< ranges<'0','9'>, ranges<'A','F'>, ranges<'a','f'> > {};
    struct UnicodeEscape : seq< one<'u'>, HexDigit, HexDigit, HexDigit, HexDigit > {};
    struct SimpleEscapeChar : one<'"','\\','/','b','f','n','r','t'> {};
    struct EscapeSequence : seq< one<'\\'>, sor< SimpleEscapeChar, UnicodeEscape > > {};
    struct StringCharacter : seq< not_at< sor< one<'"','\\'>, LineTerminator > >, pegtl::any > {};
    struct QuotedString : seq< one<'"'>, star< sor< StringCharacter, EscapeSequence > >, one<'"'> > {};

    // Block string support
    struct TripleQuote : pegtl::string<'"','"','"'> {};
    // Allow escaping of the triple quote sequence inside a block string
    struct EscapedTripleQuotes : seq< one<'\\'>, TripleQuote > {};
    // Any single char that does not start an unescaped triple quote
    struct BlockStringChar : seq< not_at< TripleQuote >, pegtl::any > {};
    struct BlockString : seq< TripleQuote, star< sor< EscapedTripleQuotes, BlockStringChar > >, TripleQuote > {};

    // Prevent fallback to QuotedString when the input starts with a triple quote
    struct StringValue : sor< BlockString, seq< not_at< TripleQuote >, QuotedString > > {};

    struct NameStart : sor<Letter,one<'_'>>{};

    struct NameContinues : sor<Letter,Digit,one<'_'>>{};

    struct Name: seq<  NameStart, star<NameContinues>  >{};

    // ===== GraphQL Token (per GraphQL spec) =====
    // Punctuator tokens in GraphQL are exactly:
    //   !  $  (  )  ...  :  =  @  [  ]  {  |  }
    // Define Ellipsis explicitly and then the set of single-character punctuators.
    struct Ellipsis : pegtl::string<'.','.','.'> {};
    struct SinglePunctuator : one<'!','$','(',')',':','=', '@','[',']','{','}','|'> {};
    struct TokenPunctuator : sor< Ellipsis, SinglePunctuator > {};

    // Token is a single lexical token: Name, IntValue, FloatValue, StringValue, or Punctuator.
    // Order matters: try longer/more specific tokens first to avoid ambiguity.
    struct Token : sor< StringValue, FloatValue, IntValue, Name, TokenPunctuator > {};

    // ===== GraphQL Ignored (per GraphQL spec) =====
    // Ignored tokens are skipped by the lexer between significant tokens and include:
    //   - UnicodeBOM (U+FEFF)
    //   - WhiteSpace (space, tab)
    //   - LineTerminator (LF, CR)
    //   - Comma ','
    //   - Comment ('#' to end-of-line)
    // Note: In UTF-8, Unicode BOM is the byte sequence 0xEF 0xBB 0xBF.
    // Use PEGTL's UTF-8 BOM rule.
    struct UnicodeBOM : pegtl::utf8::bom {};
    struct Ignored : sor< UnicodeBOM, Whitespace, LineTerminator, Comma, Comment > {};


    struct Beg : one<'{'> {
    };

    struct End : one<'}'> {
    };

    struct TSeparator : sor<Ws, Comment> {
    }; // either/or

    struct TSeps : star<TSeparator> {
    }; // any separators, whitespace or comments

    // template to generate rule
    // tao::pegtl::seq<rule0, separator, rule1, separator, rule2, ... , separator, rulen>
    template<typename TSeparator, typename... TRules>
    struct SeqWithComments;

    template<typename TSeparator, typename TRule0, typename... TRulesRest>
    struct SeqWithComments<TSeparator, TRule0, TRulesRest...>
            : seq<TRule0, TSeparator, SeqWithComments<TSeparator, TRulesRest...> > {
    };

    template<typename TSeparator, typename TRule0>
    struct SeqWithComments<TSeparator, TRule0>
            : seq<TRule0, TSeparator> {
    };

    struct GqlSubQuery : SeqWithComments<
                TSeps,
                Beg,
                opt<Name>,
                opt<GqlSubQuery>,
                End
            > {
    };

    struct GqlQuery : SeqWithComments<
                TSeps,
                opt<string<'q', 'u', 'e', 'r', 'y'> >,
                GqlSubQuery
            > {
    };

    struct GqlTypeName : SeqWithComments<
                TSeps,
                pegtl::identifier,
                TSeps
    >{};

    struct GqlStringType : SeqWithComments<
                TSeps,
                pegtl::string<'S','t','r','i','n','g'>,
                TSeps
    >{};

    struct GqlTypeInt : SeqWithComments<
                TSeps,
                pegtl::string<'I','n','t'>,
                TSeps
    >{};

    struct GqlTypeFloat : SeqWithComments<
                TSeps,
                pegtl::string<'F','l','o','a','t'>,
                TSeps
    >{};

    struct GqlTypeBoolean : SeqWithComments<
                TSeps,
                pegtl::string<'B','o','o','l','e','a','n'>,
                TSeps
    >{};

    struct GqlTypeID : SeqWithComments<
                TSeps,
                pegtl::string<'I','D'>,
                TSeps
    >{};

    struct GqlNonNullType : opt<one<'!'>>{};

    struct GqlBuiltInType;
    struct GqlType;

    struct GqlArray : SeqWithComments<
                TSeps,
                one<'['>,
                GqlType,
                one<']'>
    >{};

    struct GqlTypeRef: pegtl::identifier{};

    struct GqlType:seq<
              sor<
                    GqlArray,
                    GqlBuiltInType,
                    GqlTypeRef
              >,
              GqlNonNullType
    >{};

    struct GqlBuiltInType:sor<
                    GqlStringType,
                    GqlTypeInt,
                    GqlTypeFloat,
                    GqlTypeBoolean,
                    GqlTypeID,
                    GqlArray
                >{};

    struct GqlTypeField : SeqWithComments<
                TSeps,
                GqlTypeName,
                pegtl::string<':'>,
                GqlType
    >{};

    struct GqlTypeFields : SeqWithComments<
        TSeps,
        Beg,
        plus<GqlTypeField>,
        End
    >{};

    struct GqlTypeDef : seq<
                TSeps,
                pegtl::string<'t', 'y', 'p', 'e'>,
                plus<TSeparator>,
                GqlTypeName,
                GqlTypeFields
            > {
    };

    // ===== GraphQL Core Grammar: Document and Schema (per GraphQL.html) =====
    // Forward declarations for grammar non-terminals we reference
    struct Definition;
    struct ExecutableDefinition;
    struct OperationDefinition;
    struct FragmentDefinition;
    struct TypeSystemDefinitionOrExtension;
    struct TypeSystemDefinition;
    struct TypeSystemExtension;
    struct TypeDefinition; // map to GqlTypeDef
    struct DirectiveDefinition; // placeholder
    struct SchemaDefinition;
    struct RootOperationTypeDefinition;
    struct OperationType;
    struct VariablesDefinition; // placeholder for now
    struct Directives; // placeholder for now
    struct SelectionSet; // alias to our subquery structure
    struct Description; // placeholder (StringValue) – optional where used

    // OperationType: query | mutation | subscription
    struct OperationType : sor<
            string<'q','u','e','r','y'>,
            string<'m','u','t','a','t','i','o','n'>,
            string<'s','u','b','s','c','r','i','p','t','i','o','n'>
    >{};

    // For now, SelectionSet is equivalent to our GqlSubQuery
    struct SelectionSet : GqlSubQuery {};

    // Placeholders for future expansion (optional in places used)
    struct VariablesDefinition : seq<> {};
    struct Directives : seq<> {};
    // Description: GraphQL allows an optional description preceding many definitions.
    // It is defined lexically as a StringValue (either a quoted string or a block string).
    struct Description : StringValue {};
    struct FragmentDefinition : seq<> {};

    // OperationDefinition: (Description? OperationType Name? VariablesDefinition? Directives? SelectionSet) | SelectionSet
    struct OperationDefinition : sor<
            SeqWithComments< TSeps,
                opt<Description>,
                OperationType,
                opt<Name>,
                opt<VariablesDefinition>,
                opt<Directives>,
                SelectionSet
            >,
            SelectionSet
    >{};

    // ExecutableDefinition: OperationDefinition | FragmentDefinition
    struct ExecutableDefinition : sor< OperationDefinition, FragmentDefinition > {};

    // TypeSystemDefinitionOrExtension: TypeSystemDefinition | TypeSystemExtension
    struct TypeSystemDefinitionOrExtension : sor< struct TypeSystemDefinition, struct TypeSystemExtension > {};

    // Map TypeDefinition to our existing GqlTypeDef
    struct TypeDefinition : GqlTypeDef {};

    // Placeholder: not implemented yet
    struct DirectiveDefinition : seq<> {};
    struct TypeSystemExtension : failure {};

    // SchemaDefinition: Description? 'schema' Directives? '{' RootOperationTypeDefinition+ '}'
    struct SchemaDefinition : SeqWithComments<
            TSeps,
            opt<Description>,
            string<'s','c','h','e','m','a'>,
            opt<Directives>,
            Beg,
            plus<RootOperationTypeDefinition>,
            End
    > {};

    // RootOperationTypeDefinition: OperationType ':' NamedType
    // Use GqlTypeName as our NamedType
    struct RootOperationTypeDefinition : SeqWithComments<
            TSeps,
            OperationType,
            one<':'>,
            GqlTypeName
    > {};

    // TypeSystemDefinition: SchemaDefinition | TypeDefinition | DirectiveDefinition
    struct TypeSystemDefinition : sor< SchemaDefinition, TypeDefinition, DirectiveDefinition > {};

    // Definition: ExecutableDefinition | TypeSystemDefinitionOrExtension
    struct Definition : sor< ExecutableDefinition, TypeSystemDefinitionOrExtension > {};

    // ExecutableDocument (for completeness): ExecutableDefinition+
    struct ExecutableDocument : plus< ExecutableDefinition > {};

    // Document: Definition+
    struct Document : plus< Definition > {};

    template<typename TRule>
    using GqlSelector = pegtl::parse_tree::selector<
        TRule,
        pegtl::parse_tree::store_content::on<
            GqlQuery, Name, GqlTypeDef, GqlTypeField,GqlTypeName,GqlType,GqlTypeRef,
            GqlStringType,GqlTypeInt,GqlTypeFloat,GqlTypeBoolean,GqlTypeID,GqlArray,GqlNonNullType,
            // New grammar nodes for Document/Schema
            Definition, ExecutableDefinition, OperationDefinition, OperationType,
            SchemaDefinition, RootOperationTypeDefinition,
            // Description
            Description,
            // Numeric terminals
            IntValue, FloatValue,
            // String terminal
            StringValue,
            // Token
            Token
        >
    >;

    //template<typename> inline constexpr const char *error_message = nullptr;
    //template<> inline constexpr auto error_message<Beg> = "expected {";
    //template<> inline constexpr auto error_message<End> = "incomplete query, expected }";
    //template<> inline constexpr auto error_message<name> = "GqlError parsing name";

    //struct GqlError {
    //    template<typename Rule>
    //    static constexpr auto message = error_message<Rule>;
    //};

    //template<typename Rule>
    //using control = must_if<GqlError>::control<Rule>;

    constexpr static const char *const K_OUTPUT_SEP =
"************************************************************************";

    template<typename TGrammar>
    std::tuple<bool,std::unique_ptr<node>>
    generate_ast_and_log(const std::string &pName, pegtl::string_input<>& in, bool p_trace_on_success=false) {
        std::unique_ptr<node> myRoot = pegtl::parse_tree::parse<
            TGrammar, GqlSelector /*, ns_pegtl::nothing, control*/
        >(in);
        spdlog::debug("\n\nAST of \"{}\":\n{}", pName, K_OUTPUT_SEP);
        bool myParsingOk;
        if (myRoot) {
            pegtl::parse_tree::print_dot(std::cout, *myRoot);
            std::cout << endl
                    << K_OUTPUT_SEP << endl
                    << endl;
            myParsingOk = true;
            if (p_trace_on_success) {
                in.restart();
                tao::pegtl::complete_trace<TGrammar>(in);
            }
        } else {
            spdlog::error(R"("{}" error / no AST generated!? Input rest:"{}".)", pName,
                          std::string(in.begin(), in.size()));
            in.restart();
            tao::pegtl::complete_trace<TGrammar>(in);
            myParsingOk = false;
        }
        return {myParsingOk,std::move(myRoot)};
    }

}
#endif //ISCHED_ISCHED_GQL_GRAMMAR_HPP