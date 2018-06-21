#include <libsolidity/ast/ASTBoogieConverter.h>
#include <libsolidity/ast/BoogieAst.h>
#include <algorithm>
#include <libsolidity/interface/Exceptions.h>
#include <boost/algorithm/string/join.hpp>

using namespace std;
using namespace dev;
using namespace dev::solidity;
using namespace smack;

namespace dev
{
namespace solidity
{


string ASTBoogieConverter::replaceSpecialChars(string const& str)
{
	string ret = str;
	replace(ret.begin(), ret.end(), '.', '_');
	replace(ret.begin(), ret.end(), ':', '_');

	return ret;
}

string ASTBoogieConverter::mapType(TypePointer tp)
{
	string tpStr = tp->toString();
	// TODO: use some map instead
	// TODO: option for bit precise types
	if (tpStr == "uint256" || tpStr == "t_uint256") return "int";

	// TODO: throw some exception class instead of string
	throw "Unknown type: " + tpStr;
}

void ASTBoogieConverter::convert(ASTNode const& _node)
{
	_node.accept(*this);
}

void ASTBoogieConverter::print(ostream& _stream)
{
	program.print(_stream);
}


bool ASTBoogieConverter::visit(SourceUnit const& _node)
{
	program.getDeclarations().push_back(
			Decl::comment(
					"SourceUnit",
					"Source: " + _node.annotation().path));
    return true;
}


bool ASTBoogieConverter::visit(PragmaDirective const& _node)
{
	program.getDeclarations().push_back(
			Decl::comment(
					"PragmaDirective",
					"Pragma: " + boost::algorithm::join(_node.literals(), "")));
    return false;
}


bool ASTBoogieConverter::visit(ImportDirective const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(ContractDefinition const& _node)
{
	program.getDeclarations().push_back(
			Decl::comment(
					"ContractDefinition",
					"Contract: " + _node.fullyQualifiedName()));
	return true;
}


bool ASTBoogieConverter::visit(InheritanceSpecifier const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(UsingForDirective const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(StructDefinition const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(EnumDefinition const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(EnumValue const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(ParameterList const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(FunctionDefinition const& _node)
{
	program.getDeclarations().push_back(
			Decl::comment(
					"FunctionDefinition",
					"Function: " + _node.fullyQualifiedName()));
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(VariableDeclaration const& _node)
{
	// TODO: modifiers?
	program.getDeclarations().push_back(
			Decl::variable(replaceSpecialChars(_node.fullyQualifiedName()),
			mapType(_node.type())));
    return false;
}


bool ASTBoogieConverter::visit(ModifierDefinition const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(ModifierInvocation const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(EventDefinition const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(ElementaryTypeName const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(UserDefinedTypeName const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(FunctionTypeName const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(Mapping const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(ArrayTypeName const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(InlineAssembly const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(Block const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(PlaceholderStatement const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(IfStatement const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(WhileStatement const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(ForStatement const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(Continue const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(Break const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(Return const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(Throw const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(EmitStatement const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(VariableDeclarationStatement const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(ExpressionStatement const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(Conditional const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(Assignment const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(TupleExpression const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(UnaryOperation const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(BinaryOperation const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(FunctionCall const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(NewExpression const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(MemberAccess const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(IndexAccess const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(Identifier const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(ElementaryTypeNameExpression const& _node)
{
    return visitNode(_node);
}


bool ASTBoogieConverter::visit(Literal const& _node)
{
    return visitNode(_node);
}

bool ASTBoogieConverter::visitNode(ASTNode const&)
{
	cout << "Warning: unhandled node" << endl;
	return true;
}

}
}
