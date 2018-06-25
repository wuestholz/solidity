#include <algorithm>
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <libsolidity/ast/ASTBoogieConverter.h>
#include <libsolidity/ast/BoogieAst.h>
#include <libsolidity/interface/Exceptions.h>
#include <utility>

using namespace std;
using namespace dev;
using namespace dev::solidity;

namespace dev
{
namespace solidity
{

string ASTBoogieConverter::mapDeclName(Declaration const& decl)
{
	string name = decl.fullyQualifiedName();
	replace(name.begin(), name.end(), '.', '_');
	replace(name.begin(), name.end(), ':', '#');
	// ID is important to append, since (1) fully qualified name can be same
	// for contract variable and function local variable, (2) return variables
	// might have no name (Boogie requires a name)
	return name + "#" + to_string(decl.id());
}


string ASTBoogieConverter::mapType(TypePointer tp)
{
	string tpStr = tp->toString();
	// TODO: use some map instead
	// TODO: option for bit precise types
	if (tpStr == "uint256") return "int";
	if (tpStr == "int256") return "int";
	if (tpStr == "bool") return "bool";

	BOOST_THROW_EXCEPTION(CompilerError() << errinfo_comment("Unsupported type: " + tpStr));
	return "";
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
	// Boogie programs are flat, source units do not appear explicitly
	program.getDeclarations().push_back(
			smack::Decl::comment(
					"SourceUnit",
					"Source: " + _node.annotation().path));

	return true; // Simply apply visitor recursively
}


bool ASTBoogieConverter::visit(PragmaDirective const& _node)
{
	// Pragmas are only included as comments
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
	// Boogie programs are flat, contracts do not appear explicitly
	program.getDeclarations().push_back(
			smack::Decl::comment(
					"ContractDefinition",
					"Contract: " + _node.fullyQualifiedName()));
	return true; // Simply apply visitor recursively
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
	// Solidity functions are mapped to Boogie procedures
	program.getDeclarations().push_back(
			smack::Decl::comment(
					"FunctionDefinition",
					"Function: " + _node.fullyQualifiedName()));

	// Get input parameters
	list<smack::Binding> params;
	for (auto p = _node.parameters().begin(); p != _node.parameters().end(); ++p)
	{
		params.push_back(make_pair(
				mapDeclName(**p),
				mapType((*p)->type())));
	}

	// Get return values
	list<smack::Binding> rets;
	for (auto p = _node.returnParameters().begin(); p != _node.returnParameters().end(); ++p)
	{
		rets.push_back(make_pair(
				mapDeclName(**p),
				mapType((*p)->type())));
	}

	if (_node.returnParameters().size() > 1)
	{
		// TODO: handle multiple return values with tuple
		BOOST_THROW_EXCEPTION(InternalCompilerError() <<
				errinfo_comment("Multiple values to return is not yet supported") <<
				errinfo_sourceLocation(_node.location()));
	}
	else
	{
		currentRet = smack::Expr::id(rets.begin()->first);
	}

	// Convert function body, collect result
	localDecls.clear();
	_node.body().accept(*this);
	list<smack::Block*> blocks;
	blocks.push_back(currentBlock);

	// Local declarations were collected during processing the body
	list<smack::Decl*> decls;
	for (auto d = localDecls.begin(); d != localDecls.end(); ++d)
	{
		decls.push_back(smack::Decl::variable(
				mapDeclName(**d),
				mapType((*d)->type())));
	}

	// Create the procedure
	program.getDeclarations().push_back(
			smack::Decl::procedure(
					mapDeclName(_node), params, rets, decls, blocks));
	return false;
}


bool ASTBoogieConverter::visit(VariableDeclaration const& _node)
{
	// Local variables should be handled in the VariableDeclarationStatement
	if (_node.isLocalOrReturn())
	{
		BOOST_THROW_EXCEPTION(InternalCompilerError() <<
							errinfo_comment("Local variable appearing in VariableDeclaration") <<
							errinfo_sourceLocation(_node.location()));
	}
	if (_node.value())
	{
		BOOST_THROW_EXCEPTION(CompilerError() <<
							errinfo_comment("Initial values are not supported yet") <<
							errinfo_sourceLocation(_node.location()));
	}

	program.getDeclarations().push_back(
						smack::Decl::variable(mapDeclName(_node),
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
	// TODO: handle nested blocks (with e.g. a stack)
	// Create new empty block
	currentBlock = smack::Block::block();
	// Simply apply visitor recursively
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
	// Get rhs recursively
	currentExpr = nullptr;
	_node.expression()->accept(*this);
	const smack::Expr* rhs = currentExpr;

	// lhs should already be known
	const smack::Expr* lhs = currentRet;

	// First create an assignment, and then an empty return
	currentBlock->addStmt(smack::Stmt::assign(lhs, rhs));
	currentBlock->addStmt(smack::Stmt::return_());

	return false;
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
	// TODO: modifiers?
	for (auto decl = _node.declarations().begin(); decl != _node.declarations().end(); decl++)
	{
		if ((*decl)->isLocalOrReturn())
		{
			// Boogie requires local variables to be declared at the beginning of the procedure
			localDecls.push_back(*decl);
		}
		else
		{
			// Non-local variables should be handled as VariableDeclaration
			BOOST_THROW_EXCEPTION(InternalCompilerError() <<
					errinfo_comment("Non-local variable appearing in VariableDeclarationStatement") <<
					errinfo_sourceLocation(_node.location()));
		}
	}

	// Convert initial value into an assignment statement
	if (_node.initialValue())
	{
		if (_node.declarations().size() == 1)
		{
			// Get expression recursively
			currentExpr = nullptr;
			_node.initialValue()->accept(*this);

			currentBlock->addStmt(smack::Stmt::assign(
					smack::Expr::id(mapDeclName(*(_node.declarations()[0]))),
					currentExpr));
		}
		else
		{
			BOOST_THROW_EXCEPTION(CompilerError() <<
					errinfo_comment("Assignment to multiple variables is not supported yet") <<
					errinfo_sourceLocation(_node.location()));
		}
	}
	return false;
}


bool ASTBoogieConverter::visit(ExpressionStatement const& _node)
{
	if (dynamic_cast<Assignment const*>(&_node.expression())) return true;

	BOOST_THROW_EXCEPTION(CompilerError() <<
				errinfo_comment(string("Unsupported ExpressionStatement")) <<
				errinfo_sourceLocation(_node.location()));
	return false;
}


bool ASTBoogieConverter::visit(Conditional const& _node)
{
	return visitNode(_node);
}


bool ASTBoogieConverter::visit(Assignment const& _node)
{
	// Get lhs recursively
	currentExpr = nullptr;
	_node.leftHandSide().accept(*this);
	const smack::Expr* lhs = currentExpr;

	// Get rhs recursively
	currentExpr = nullptr;
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
	// Get operand recursively
	currentExpr = nullptr;
	_node.subExpression().accept(*this);
	const smack::Expr* subExpr = currentExpr;

	switch (_node.getOperator()) {
	case Token::Not: currentExpr = smack::Expr::not_(subExpr); break;
	case Token::Sub: currentExpr = smack::Expr::minus(subExpr); break;

	default: BOOST_THROW_EXCEPTION(InternalCompilerError() <<
			errinfo_comment(string("Unsupported operator ") + Token::toString(_node.getOperator())) <<
			errinfo_sourceLocation(_node.location()));
	}

	return false;
}


bool ASTBoogieConverter::visit(BinaryOperation const& _node)
{
	// Get lhs recursively
	currentExpr = nullptr;
	_node.leftExpression().accept(*this);
	const smack::Expr* lhs = currentExpr;

	// Get rhs recursively
	currentExpr = nullptr;
	_node.rightExpression().accept(*this);
	const smack::Expr* rhs = currentExpr;

	switch(_node.getOperator())
	{
	case Token::And: currentExpr = smack::Expr::and_(lhs, rhs); break;
	case Token::Or: currentExpr = smack::Expr::or_(lhs, rhs); break;

	case Token::Add: currentExpr = smack::Expr::plus(lhs, rhs); break;
	case Token::Sub: currentExpr = smack::Expr::minus(lhs, rhs); break;
	case Token::Mul: currentExpr = smack::Expr::times(lhs, rhs); break;
	case Token::Div: // Boogie has different division operators for integers and reals
		if (mapType(_node.annotation().type) == "int") currentExpr = smack::Expr::intdiv(lhs, rhs);
		else currentExpr = smack::Expr::div(lhs, rhs);
		break;
	case Token::Mod: currentExpr = smack::Expr::mod(lhs, rhs); break;

	case Token::Equal: currentExpr = smack::Expr::eq(lhs, rhs); break;
	case Token::NotEqual: currentExpr = smack::Expr::neq(lhs, rhs); break;
	case Token::LessThan: currentExpr = smack::Expr::lt(lhs, rhs); break;
	case Token::GreaterThan: currentExpr = smack::Expr::gt(lhs, rhs); break;
	case Token::LessThanOrEqual: currentExpr = smack::Expr::lte(lhs, rhs); break;
	case Token::GreaterThanOrEqual: currentExpr = smack::Expr::gte(lhs, rhs); break;

	default: BOOST_THROW_EXCEPTION(InternalCompilerError() <<
			errinfo_comment(string("Unsupported operator ") + Token::toString(_node.getOperator())) <<
			errinfo_sourceLocation(_node.location()));
	}

	return false;
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
	string declName = mapDeclName(*(_node.annotation().referencedDeclaration));
	currentExpr = smack::Expr::id(declName);
	return false;
}


bool ASTBoogieConverter::visit(ElementaryTypeNameExpression const& _node)
{
	return visitNode(_node);
}


bool ASTBoogieConverter::visit(Literal const& _node)
{
	string tpStr = _node.annotation().type->toString();
	// TODO: use some map instead
	// TODO: option for bit precise types
	if (boost::starts_with(tpStr, "int_const"))
	{
		currentExpr = smack::Expr::lit(stol(_node.value()));
		return false;
	}

	BOOST_THROW_EXCEPTION(CompilerError() << errinfo_comment("Unsupported type: " + tpStr));
	return false;
}

bool ASTBoogieConverter::visitNode(ASTNode const& _node)
{
	cout << "Warning: unhandled node at " << _node.location().start << ":" << _node.location().end << endl;
	return true;
}

}
}
