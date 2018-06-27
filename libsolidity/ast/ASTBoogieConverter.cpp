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

const string ASSERT_NAME = "assert";
const string VERIFIER_MAIN_NAME = "__verifier_main";

string ASTBoogieConverter::mapDeclName(Declaration const& decl)
{
	if (decl.name() == VERIFIER_MAIN_NAME) return "main";

	string name = decl.name();
	replace(name.begin(), name.end(), '.', '_');
	replace(name.begin(), name.end(), ':', '#');
	// ID is important to append, since (1) even fully qualified names can be
	// same for contract variable and function local variable, (2) return
	// variables might have no name (Boogie requires a name)
	return name + "#" + to_string(decl.id());
}

string ASTBoogieConverter::mapType(TypePointer tp, ASTNode const& _associatedNode)
{
	// TODO: option for bit precise types
	string tpStr = tp->toString();
	if (tpStr == "address") return "address";
	if (tpStr == "bool") return "bool";
	for (int i = 8; i <= 256; ++i)
	{
		if (tpStr == "int" + to_string(i)) return "int";
		if (tpStr == "uint" + to_string(i)) return "int";
	}

	BOOST_THROW_EXCEPTION(CompilerError() <<
			errinfo_comment("Unsupported type: " + tpStr) <<
			errinfo_sourceLocation(_associatedNode.location()));
	return "";
}

ASTBoogieConverter::ASTBoogieConverter()
{
	currentExpr = nullptr;
	currentRet = nullptr;
	program.getDeclarations().push_back(
			smack::Decl::comment("", "Uninterpreted type for addresses"));
	program.getDeclarations().push_back(smack::Decl::typee("address"));
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
	// Boogie programs are flat, inheritance does not appear explicitly
	program.getDeclarations().push_back(
			smack::Decl::comment(
					"InheritanceSpecifier",
					"Inherits from: " + boost::algorithm::join(_node.name().namePath(), "#")));
	return false;
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
	for (auto p : _node.parameters())
	{
		params.push_back(make_pair(
				mapDeclName(*p),
				mapType(p->type(), *p)));
	}

	// Get return values
	list<smack::Binding> rets;
	for (auto p : _node.returnParameters())
	{
		rets.push_back(make_pair(
				mapDeclName(*p),
				mapType(p->type(), *p)));
	}

	if (_node.returnParameters().size() > 1)
	{
		// TODO: handle multiple return values with tuple
		BOOST_THROW_EXCEPTION(CompilerError() <<
				errinfo_comment("Multiple values to return is not yet supported") <<
				errinfo_sourceLocation(_node.location()));
	}
	else
	{
		currentRet = smack::Expr::id(rets.begin()->first);
	}

	// Convert function body, collect result
	localDecls.clear();
	// Create new empty block
	currentBlocks.push(smack::Block::block());
	_node.body().accept(*this);
	list<smack::Block*> blocks;
	blocks.push_back(currentBlocks.top());
	currentBlocks.pop();
	if (!currentBlocks.empty())
	{
		BOOST_THROW_EXCEPTION(InternalCompilerError() <<
				errinfo_comment("Non-empty stack of blocks at the end of function."));
	}

	// Create the procedure
	program.getDeclarations().push_back(
			smack::Decl::procedure(
					mapDeclName(_node), params, rets, localDecls, blocks));
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
						mapType(_node.type(), _node)));
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
	// Simply apply visitor recursively, compound statements will
	// handle creating new blocks
	return true;
}

bool ASTBoogieConverter::visit(PlaceholderStatement const& _node)
{
	return visitNode(_node);
}


bool ASTBoogieConverter::visit(IfStatement const& _node)
{
	// Get condition recursively
	currentExpr = nullptr;
	_node.condition().accept(*this);
	const smack::Expr* cond = currentExpr;

	// Get if branch recursively
	currentBlocks.push(smack::Block::block());
	_node.trueStatement().accept(*this);
	const smack::Block* then = currentBlocks.top();
	currentBlocks.pop();

	// Get else branch recursively
	const smack::Block* elze = nullptr;
	if (_node.falseStatement())
	{
		currentBlocks.push(smack::Block::block());
		_node.falseStatement()->accept(*this);
		elze = currentBlocks.top();
		currentBlocks.pop();
	}

	currentBlocks.top()->addStmt(smack::Stmt::ifelse(cond, then, elze));
	return false;
}


bool ASTBoogieConverter::visit(WhileStatement const& _node)
{
	// Get condition recursively
	currentExpr = nullptr;
	_node.condition().accept(*this);
	const smack::Expr* cond = currentExpr;

	// Get if branch recursively
	currentBlocks.push(smack::Block::block());
	_node.body().accept(*this);
	const smack::Block* body = currentBlocks.top();
	currentBlocks.pop();

	// TODO: loop invariants can be added here

	currentBlocks.top()->addStmt(smack::Stmt::while_(cond, body));

	return false;
}


bool ASTBoogieConverter::visit(ForStatement const& _node)
{
	// Boogie does not have a for statement, therefore it is transformed
	// into a while statement in the following way:
	//
	// for (initExpr; cond; loopExpr) { body }
	//
	// initExpr; while (cond) { body; loopExpr }

	// Get initialization recursively (adds statement to current block)
	currentBlocks.top()->addStmt(smack::Stmt::comment("The following while loop was mapped from a for loop"));
	if (_node.initializationExpression())
	{
		_node.initializationExpression()->accept(*this);
	}

	// Get condition recursively
	currentExpr = nullptr;
	if (_node.condition())
	{
		_node.condition()->accept(*this);
	}
	const smack::Expr* cond = currentExpr;

	// Get body recursively
	currentBlocks.push(smack::Block::block());
	_node.body().accept(*this);
	// Include loop expression at the end of body
	if (_node.loopExpression())
	{
		_node.loopExpression()->accept(*this);
	}
	const smack::Block* body = currentBlocks.top();
	currentBlocks.pop();

	// TODO: loop invariants can be added here

	currentBlocks.top()->addStmt(smack::Stmt::while_(cond, body));

	return false;
}


bool ASTBoogieConverter::visit(Continue const& _node)
{
	// TODO: Boogie does not support continue, this must be mapped manually
	// using labels and gotos
	return visitNode(_node);
}


bool ASTBoogieConverter::visit(Break const&)
{
	currentBlocks.top()->addStmt(smack::Stmt::break_());
	return false;
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
	currentBlocks.top()->addStmt(smack::Stmt::assign(lhs, rhs));
	currentBlocks.top()->addStmt(smack::Stmt::return_());

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
	for (auto decl : _node.declarations())
	{
		if (decl->isLocalOrReturn())
		{
			// Boogie requires local variables to be declared at the beginning of the procedure
			localDecls.push_back(smack::Decl::variable(
							mapDeclName(*decl),
							mapType(decl->type(), *decl)));
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

			currentBlocks.top()->addStmt(smack::Stmt::assign(
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
	// Boogie cannot have expressions as statements, therefore
	// ExpressionStatements have to be checked if they have a corresponding
	// statement in Boogie
	if (dynamic_cast<Assignment const*>(&_node.expression())) return true;
	if (dynamic_cast<FunctionCall const*>(&_node.expression())) return true;

	BOOST_THROW_EXCEPTION(CompilerError() <<
				errinfo_comment(string("Unsupported ExpressionStatement")) <<
				errinfo_sourceLocation(_node.location()));
	return false;
}


bool ASTBoogieConverter::visit(Conditional const& _node)
{
	// Get condition recursively
	currentExpr = nullptr;
	_node.condition().accept(*this);
	const smack::Expr* cond = currentExpr;

	// Get true expression recursively
	currentExpr = nullptr;
	_node.trueExpression().accept(*this);
	const smack::Expr* trueExpr = currentExpr;

	// Get false expression recursively
	currentExpr = nullptr;
	_node.falseExpression().accept(*this);
	const smack::Expr* falseExpr = currentExpr;

	currentExpr = smack::Expr::cond(cond, trueExpr, falseExpr);
	return false;
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

	// Transform rhs based on the operator, e.g., a += b becomes a := a + b
	switch (_node.assignmentOperator()) {
	case Token::Assign: break; // rhs already contains the result
	case Token::AssignAdd: rhs = smack::Expr::plus(lhs, rhs); break;
	case Token::AssignSub: rhs = smack::Expr::minus(lhs, rhs); break;
	case Token::AssignMul: rhs = smack::Expr::times(lhs, rhs); break;
	case Token::AssignDiv:
		if (mapType(_node.annotation().type, _node) == "int") rhs = smack::Expr::intdiv(lhs, rhs);
		else rhs = smack::Expr::div(lhs, rhs);
		break;
	case Token::AssignMod: rhs = smack::Expr::mod(lhs, rhs); break;

	default: BOOST_THROW_EXCEPTION(CompilerError() <<
			errinfo_comment(string("Unsupported assignment operator ") + Token::toString(_node.assignmentOperator())) <<
			errinfo_sourceLocation(_node.location()));
	}

	currentBlocks.top()->addStmt(smack::Stmt::assign(lhs, rhs));


	return false;
}


bool ASTBoogieConverter::visit(TupleExpression const& _node)
{
	if (_node.components().size() == 1)
	{
		_node.components()[0]->accept(*this);
	}
	else
	{
		BOOST_THROW_EXCEPTION(CompilerError() <<
				errinfo_comment("Tuples are not supported yet") <<
				errinfo_sourceLocation(_node.location()));
	}
	return false;
}


bool ASTBoogieConverter::visit(UnaryOperation const& _node)
{
	// Get operand recursively
	currentExpr = nullptr;
	_node.subExpression().accept(*this);
	const smack::Expr* subExpr = currentExpr;

	switch (_node.getOperator()) {
	case Token::Not: currentExpr = smack::Expr::not_(subExpr); break;
	case Token::Sub: currentExpr = smack::Expr::neg(subExpr); break;

	default: BOOST_THROW_EXCEPTION(CompilerError() <<
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
		if (mapType(_node.annotation().type, _node) == "int") currentExpr = smack::Expr::intdiv(lhs, rhs);
		else currentExpr = smack::Expr::div(lhs, rhs);
		break;
	case Token::Mod: currentExpr = smack::Expr::mod(lhs, rhs); break;

	case Token::Equal: currentExpr = smack::Expr::eq(lhs, rhs); break;
	case Token::NotEqual: currentExpr = smack::Expr::neq(lhs, rhs); break;
	case Token::LessThan: currentExpr = smack::Expr::lt(lhs, rhs); break;
	case Token::GreaterThan: currentExpr = smack::Expr::gt(lhs, rhs); break;
	case Token::LessThanOrEqual: currentExpr = smack::Expr::lte(lhs, rhs); break;
	case Token::GreaterThanOrEqual: currentExpr = smack::Expr::gte(lhs, rhs); break;

	case Token::Exp:
		// Solidity supports exp only for integers whereas Boogie only
		// supports it for reals. Could be worked around with casts.
	default: BOOST_THROW_EXCEPTION(CompilerError() <<
			errinfo_comment(string("Unsupported operator ") + Token::toString(_node.getOperator())) <<
			errinfo_sourceLocation(_node.location()));
	}

	return false;
}


bool ASTBoogieConverter::visit(FunctionCall const& _node)
{
	// In Boogie function calls are statements and cannot be part of
	// expressions, therefore each function call is given a fresh variable
	// and is mapped to a call statement

	// Get arguments recursively
	list<const smack::Expr*> args;
	for (auto arg : _node.arguments())
	{
		currentExpr = nullptr;
		arg->accept(*this);
		args.push_back(currentExpr);
	}

	// Get the name of the called function
	currentExpr = nullptr;
	_node.expression().accept(*this);
	string funcName;
	if (smack::VarExpr const * v = dynamic_cast<smack::VarExpr const*>(currentExpr))
	{
		funcName = v->name();
	}
	else
	{
		BOOST_THROW_EXCEPTION(CompilerError() <<
				errinfo_comment(string("Only identifiers are supported as function calls")) <<
				errinfo_sourceLocation(_node.location()));
	}

	// Assert is a separate statement in Boogie (instead of a function call)
	if (funcName == ASSERT_NAME)
	{
		currentBlocks.top()->addStmt(smack::Stmt::assert_(*args.begin()));
		return false;
	}

	// TODO: check for void return in a more appropriate way
	if (_node.annotation().type->toString() != "tuple()")
	{
		// Create fresh variable to store the result
		smack::Decl* returnVar = smack::Decl::variable(funcName + "#" + to_string(_node.id()), mapType(_node.annotation().type, _node));
		localDecls.push_back(returnVar);
		// Result of the function call is the fresh variable
		currentExpr = smack::Expr::id(returnVar->getName());
		list<string> rets;
		rets.push_back(returnVar->getName());
		// Assign to the fresh variable
		currentBlocks.top()->addStmt(smack::Stmt::call(funcName, args, rets));
	}
	else
	{
		currentExpr = nullptr; // No return value for function
		currentBlocks.top()->addStmt(smack::Stmt::call(funcName, args));
	}

	return false;
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
	if (_node.name() == ASSERT_NAME)
	{
		currentExpr = smack::Expr::id(_node.name());
	}
	else
	{
		string declName = mapDeclName(*(_node.annotation().referencedDeclaration));
		currentExpr = smack::Expr::id(declName);
	}
	return false;
}


bool ASTBoogieConverter::visit(ElementaryTypeNameExpression const& _node)
{
	return visitNode(_node);
}


bool ASTBoogieConverter::visit(Literal const& _node)
{
	string tpStr = _node.annotation().type->toString();
	// TODO: option for bit precise types
	if (boost::starts_with(tpStr, "int_const"))
	{
		currentExpr = smack::Expr::lit(stol(_node.value()));
		return false;
	}
	if (tpStr == "bool")
	{
		// TODO: case insensitve?
		currentExpr = smack::Expr::lit(_node.value() == "true");
		return false;
	}

	BOOST_THROW_EXCEPTION(CompilerError() <<
			errinfo_comment("Unsupported type: " + tpStr) <<
			errinfo_sourceLocation(_node.location()));
	return false;
}

bool ASTBoogieConverter::visitNode(ASTNode const& _node)
{
	BOOST_THROW_EXCEPTION(InternalCompilerError() <<
					errinfo_comment("Unhandled node") <<
					errinfo_sourceLocation(_node.location()));
	return true;
}

}
}
