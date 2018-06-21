#include <libsolidity/ast/ASTBoogieConverter.h>
#include <libsolidity/ast/BoogieAst.h>
#include <algorithm>
#include <libsolidity/interface/Exceptions.h>
#include <boost/algorithm/string/join.hpp>
#include <utility>

using namespace std;
using namespace dev;
using namespace dev::solidity;

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
	if (tpStr == "uint256") return "int";

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
			smack::Decl::comment(
					"SourceUnit",
					"Source: " + _node.annotation().path));
    return true;
}


bool ASTBoogieConverter::visit(PragmaDirective const& _node)
{
	program.getDeclarations().push_back(
			smack::Decl::comment(
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
			smack::Decl::comment(
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
			smack::Decl::comment(
					"FunctionDefinition",
					"Function: " + _node.fullyQualifiedName()));

	list<smack::Binding> params;
	for (auto p = _node.parameters().begin(); p != _node.parameters().end(); ++p)
	{
		params.push_back(make_pair(
				replaceSpecialChars((*p)->fullyQualifiedName()),
				mapType((*p)->type())));
	}

	list<smack::Binding> rets;
	for (auto p = _node.returnParameters().begin(); p != _node.returnParameters().end(); ++p)
	{
		// TODO: return variables might have no name
		rets.push_back(make_pair(
				replaceSpecialChars((*p)->fullyQualifiedName()),
				mapType((*p)->type())));
	}

	list<smack::Decl*> decls;
	// TODO: collect all local variables from the function

	_node.body().accept(*this);
	list<smack::Block*> blocks;
	blocks.push_back(currentBlock);

	program.getDeclarations().push_back(
			smack::Decl::procedure(
					replaceSpecialChars(_node.fullyQualifiedName()), params, rets, decls, blocks));
    return false;
}


bool ASTBoogieConverter::visit(VariableDeclaration const& _node)
{
	// TODO: modifiers?
	// TODO: initializer expression?
	program.getDeclarations().push_back(
			smack::Decl::variable(replaceSpecialChars(_node.fullyQualifiedName()),
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


bool ASTBoogieConverter::visit(Block const&)
{
	currentBlock = smack::Block::block();
    return true;
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
	currentBlock->addStmt(smack::Stmt::comment("Return."));
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
	_node.leftHandSide().accept(*this);
	const smack::Expr* lhs = currentExpr;

	_node.rightHandSide().accept(*this);
	const smack::Expr* rhs = currentExpr;

	currentBlock->addStmt(smack::Stmt::assign(lhs, rhs));

	return false;
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
	string declName = replaceSpecialChars(_node.annotation().referencedDeclaration->fullyQualifiedName());
	currentExpr = smack::Expr::id(declName);
    return false;
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
