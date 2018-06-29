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

const string SOLIDITY_ADDRESS_TYPE = "address";
const string BOOGIE_ADDRESS_TYPE = "address_t";
const string SOLIDITY_BALANCE = "balance";
const string BOOGIE_BALANCE = "__balance";
const string SOLIDITY_MSG = "msg";
const string SOLIDITY_SENDER = "sender";
const string BOOGIE_MSG_SENDER = "__msg_sender";
const string SOLIDITY_TRANSFER = "transfer";
const string BOOGIE_TRANSFER = "__transfer";
const string SOLIDITY_THIS = "this";
const string BOOGIE_THIS = "__this";
const string SOLIDITY_ASSERT = "assert";
const string VERIFIER_MAIN = "__verifier_main";
const string BOOGIE_CONSTRUCTOR = "__constructor";

void ASTBoogieConverter::addGlobalComment(string str)
{
	program.getDeclarations().push_back(smack::Decl::comment("", str));
}

string ASTBoogieConverter::mapDeclName(Declaration const& decl)
{
	// Check for special names
	if (decl.name() == VERIFIER_MAIN) return "main";
	if (decl.name() == SOLIDITY_ASSERT) return "assert";
	if (decl.name() == SOLIDITY_THIS) return BOOGIE_THIS;

	// ID is important to append, since (1) even fully qualified names can be
	// same for state variables and local variables in functions, (2) return
	// variables might have no name (whereas Boogie requires a name)
	return decl.name() + "#" + to_string(decl.id());
}

string ASTBoogieConverter::mapType(TypePointer tp, ASTNode const& _associatedNode)
{
	// TODO: option for bit precise types
	string tpStr = tp->toString();
	if (tpStr == SOLIDITY_ADDRESS_TYPE) return BOOGIE_ADDRESS_TYPE;
	if (tpStr == "bool") return "bool";
	for (int i = 8; i <= 256; ++i)
	{
		if (tpStr == "int" + to_string(i)) return "int";
		if (tpStr == "uint" + to_string(i)) return "int";
	}
	if (boost::algorithm::starts_with(tpStr, "contract ")) return BOOGIE_ADDRESS_TYPE;

	BOOST_THROW_EXCEPTION(CompilerError() <<
			errinfo_comment("Unsupported type: " + tpStr) <<
			errinfo_sourceLocation(_associatedNode.location()));
	return "";
}

ASTBoogieConverter::ASTBoogieConverter()
{
	currentExpr = nullptr;
	currentRet = nullptr;
	// Initialize global declarations
	addGlobalComment("Global declarations and definitions related to the address type");
	// address type
	program.getDeclarations().push_back(smack::Decl::typee(BOOGIE_ADDRESS_TYPE));
	// address.balance
	program.getDeclarations().push_back(smack::Decl::variable(BOOGIE_BALANCE, "[" + BOOGIE_ADDRESS_TYPE + "]int"));
	// address.transfer()
	list<smack::Binding> transferParams;
	transferParams.push_back(make_pair(BOOGIE_THIS, BOOGIE_ADDRESS_TYPE)); // 'this'
	transferParams.push_back(make_pair(BOOGIE_MSG_SENDER, BOOGIE_ADDRESS_TYPE)); // 'msg.sender'
	transferParams.push_back(make_pair("amount", "int"));
	smack::Block* transferImpl = smack::Block::block();
	// balance[this] += amount
	transferImpl->addStmt(smack::Stmt::assign(
			smack::Expr::id(BOOGIE_BALANCE),
			smack::Expr::upd(
					smack::Expr::id(BOOGIE_BALANCE),
					smack::Expr::id(BOOGIE_THIS),
					smack::Expr::plus(smack::Expr::sel(BOOGIE_BALANCE, BOOGIE_THIS), smack::Expr::id("amount"))
			)));
	// balance[msg.sender] -= amount
	transferImpl->addStmt(smack::Stmt::assign(
			smack::Expr::id(BOOGIE_BALANCE),
			smack::Expr::upd(
					smack::Expr::id(BOOGIE_BALANCE),
					smack::Expr::id(BOOGIE_MSG_SENDER),
					smack::Expr::minus(smack::Expr::sel(BOOGIE_BALANCE, BOOGIE_MSG_SENDER), smack::Expr::id("amount"))
			)));
	transferImpl->addStmt(smack::Stmt::comment("TODO: call fallback, exception"));
	list<smack::Block*> transferBlocks;
	transferBlocks.push_back(transferImpl);
	program.getDeclarations().push_back(smack::Decl::procedure(
			BOOGIE_TRANSFER, transferParams, list<smack::Binding>(), list<smack::Decl*>(), transferBlocks));
}

void ASTBoogieConverter::convert(ASTNode const& _node)
{
	_node.accept(*this);
}

void ASTBoogieConverter::print(ostream& _stream)
{
	program.print(_stream);
}

// ---------------------------------------------------------------------------
//         Visitor methods for top-level nodes and declarations
// ---------------------------------------------------------------------------

bool ASTBoogieConverter::visit(SourceUnit const& _node)
{
	// Boogie programs are flat, source units do not appear explicitly
	addGlobalComment("");
	addGlobalComment("------- Source: " + _node.annotation().path + " -------");
	return true; // Simply apply visitor recursively
}

bool ASTBoogieConverter::visit(PragmaDirective const& _node)
{
	// Pragmas are only included as comments
	addGlobalComment("Pragma: " + boost::algorithm::join(_node.literals(), ""));
	return false;
}

bool ASTBoogieConverter::visit(ImportDirective const& _node)
{
	return visitNode(_node);
}

bool ASTBoogieConverter::visit(ContractDefinition const& _node)
{
	// Boogie programs are flat, contracts do not appear explicitly
	addGlobalComment("");
	addGlobalComment("------- Contract: " + _node.name() + " -------");
	return true; // Simply apply visitor recursively
}

bool ASTBoogieConverter::visit(InheritanceSpecifier const& _node)
{
	// Boogie programs are flat, inheritance does not appear explicitly
	addGlobalComment("Inherits from: " + boost::algorithm::join(_node.name().namePath(), "#"));
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
	addGlobalComment("");
	addGlobalComment("Function: " + _node.name() + " : " + _node.type()->toString());

	// Input parameters
	list<smack::Binding> params;
	// Add some extra parameters for globally available variables
	params.push_back(make_pair(BOOGIE_THIS, BOOGIE_ADDRESS_TYPE)); // this
	params.push_back(make_pair(BOOGIE_MSG_SENDER, BOOGIE_ADDRESS_TYPE)); // msg.sender
	// Add original parameters of the function
	for (auto p : _node.parameters())
	{
		params.push_back(make_pair(mapDeclName(*p), mapType(p->type(), *p)));
	}

	// Return values
	list<smack::Binding> rets;
	for (auto p : _node.returnParameters())
	{
		rets.push_back(make_pair(mapDeclName(*p), mapType(p->type(), *p)));
	}

	// Boogie treats return as an assignment to the return variable(s)
	if (_node.returnParameters().empty())
	{
		currentRet = nullptr;
	}
	else if (_node.returnParameters().size() == 1)
	{
		currentRet = smack::Expr::id(rets.begin()->first);
	}
	else
	{
		BOOST_THROW_EXCEPTION(CompilerError() <<
				errinfo_comment("Multiple values to return is not yet supported") <<
				errinfo_sourceLocation(_node.location()));
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

	// Get the name of the function
	string funcName = _node.isConstructor() ? BOOGIE_CONSTRUCTOR : mapDeclName(_node);

	// Create the procedure
	program.getDeclarations().push_back(smack::Decl::procedure(funcName, params, rets, localDecls, blocks));
	return false;
}

bool ASTBoogieConverter::visit(VariableDeclaration const& _node)
{
	// Non-state variables should be handled in the VariableDeclarationStatement
	if (!_node.isStateVariable())
	{
		BOOST_THROW_EXCEPTION(InternalCompilerError() <<
				errinfo_comment("Non-state variable appearing in VariableDeclaration") <<
				errinfo_sourceLocation(_node.location()));
	}
	if (_node.value())
	{
		BOOST_THROW_EXCEPTION(CompilerError() <<
				errinfo_comment("Initial values are not supported yet") <<
				errinfo_sourceLocation(_node.location()));
	}

	addGlobalComment("");
	addGlobalComment("State variable: " + _node.name() + " : " + _node.type()->toString());
	// State variables are represented as maps from address to their type
	program.getDeclarations().push_back(
			smack::Decl::variable(mapDeclName(_node),
			"[" + BOOGIE_ADDRESS_TYPE + "]" + mapType(_node.type(), _node)));
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

// ---------------------------------------------------------------------------
//                     Visitor methods for statements
// ---------------------------------------------------------------------------

bool ASTBoogieConverter::visit(InlineAssembly const& _node)
{
	return visitNode(_node);
}

bool ASTBoogieConverter::visit(Block const&)
{
	// Simply apply visitor recursively, compound statements will
	// handle creating new blocks when required
	return true;
}

bool ASTBoogieConverter::visit(PlaceholderStatement const& _node)
{
	return visitNode(_node);
}

bool ASTBoogieConverter::visit(IfStatement const& _node)
{
	// Get condition recursively
	_node.condition().accept(*this);
	const smack::Expr* cond = currentExpr;

	// Get true branch recursively
	currentBlocks.push(smack::Block::block());
	_node.trueStatement().accept(*this);
	const smack::Block* thenBlock = currentBlocks.top();
	currentBlocks.pop();

	// Get false branch recursively
	const smack::Block* elseBlock = nullptr;
	if (_node.falseStatement())
	{
		currentBlocks.push(smack::Block::block());
		_node.falseStatement()->accept(*this);
		elseBlock = currentBlocks.top();
		currentBlocks.pop();
	}

	currentBlocks.top()->addStmt(smack::Stmt::ifelse(cond, thenBlock, elseBlock));
	return false;
}

bool ASTBoogieConverter::visit(WhileStatement const& _node)
{
	// Get condition recursively
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
	const smack::Expr* cond = nullptr;
	if (_node.condition())
	{
		_node.condition()->accept(*this);
		cond = currentExpr;
	}

	// Get body recursively
	currentBlocks.push(smack::Block::block());
	_node.body().accept(*this);
	// Include loop expression at the end of body
	if (_node.loopExpression())
	{
		_node.loopExpression()->accept(*this); // Adds statement to current block
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
	// No return value
	if (_node.expression() == nullptr)
	{
		currentBlocks.top()->addStmt(smack::Stmt::return_());
	}
	else
	{
		// Get rhs recursively
		_node.expression()->accept(*this);
		const smack::Expr* rhs = currentExpr;

		// lhs should already be known (set by the enclosing FunctionDefinition)
		const smack::Expr* lhs = currentRet;

		// First create an assignment, and then an empty return
		currentBlocks.top()->addStmt(smack::Stmt::assign(lhs, rhs));
		currentBlocks.top()->addStmt(smack::Stmt::return_());
	}
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
	for (auto decl : _node.declarations())
	{
		if (decl->isLocalVariable())
		{
			// Boogie requires local variables to be declared at the beginning of the procedure
			localDecls.push_back(smack::Decl::variable(
							mapDeclName(*decl),
							mapType(decl->type(), *decl)));
		}
		else
		{
			// Non-local variables should be handled elsewhere
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
			_node.initialValue()->accept(*this);
			const smack::Expr* rhs = currentExpr;

			currentBlocks.top()->addStmt(smack::Stmt::assign(
					smack::Expr::id(mapDeclName(**_node.declarations().begin())),
					rhs));
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
				errinfo_comment(string("Unsupported expression appearing as statement")) <<
				errinfo_sourceLocation(_node.location()));
	return false;
}

// ---------------------------------------------------------------------------
//                    Visitor methods for expressions
// ---------------------------------------------------------------------------

bool ASTBoogieConverter::visit(Conditional const& _node)
{
	// Get condition recursively
	_node.condition().accept(*this);
	const smack::Expr* cond = currentExpr;

	// Get true expression recursively
	_node.trueExpression().accept(*this);
	const smack::Expr* trueExpr = currentExpr;

	// Get false expression recursively
	_node.falseExpression().accept(*this);
	const smack::Expr* falseExpr = currentExpr;

	currentExpr = smack::Expr::cond(cond, trueExpr, falseExpr);
	return false;
}

bool ASTBoogieConverter::visit(Assignment const& _node)
{
	// Check if LHS is an identifier referencing a variable
	if (Identifier const* lhsId = dynamic_cast<Identifier const*>(&_node.leftHandSide()))
	{
		if (const VariableDeclaration* varDecl = dynamic_cast<const VariableDeclaration*>(lhsId->annotation().referencedDeclaration))
		{
			// Get lhs recursively (required for +=, *=, etc.)
			_node.leftHandSide().accept(*this);
			const smack::Expr* lhs = currentExpr;

			// Get rhs recursively
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

			// State variables are represented by maps, so x = 5 becomes
			// an assignment and a map update like x := x[this := 5]
			if (varDecl->isStateVariable())
			{
				currentBlocks.top()->addStmt(smack::Stmt::assign(
						smack::Expr::id(mapDeclName(*varDecl)),
						smack::Expr::upd(
								smack::Expr::id(mapDeclName(*varDecl)),
								smack::Expr::id(BOOGIE_THIS),
								rhs)));
			}
			else // Non-state variables can be simply assigned
			{
				currentBlocks.top()->addStmt(smack::Stmt::assign(lhs, rhs));
			}
			return false;
		}
	}

	BOOST_THROW_EXCEPTION(CompilerError() <<
			errinfo_comment("Only variable references are supported as LHS in assignment") <<
			errinfo_sourceLocation(_node.location()));
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
	_node.leftExpression().accept(*this);
	const smack::Expr* lhs = currentExpr;

	// Get rhs recursively
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
	if (_node.annotation().kind == FunctionCallKind::TypeConversion)
	{
		// TODO: type conversions are currently omitted, but might be needed
		// for basic types such as int
		(*_node.arguments().begin())->accept(*this);
		return false;
	}
	// Function calls in Boogie are statements and cannot be part of
	// expressions, therefore each function call is given a fresh variable
	// for its return value and is mapped to a call statement

	// Get the name of the called function and the address on which it is called
	// By default, it is 'this', but member access expressions can override it
	currentExpr = nullptr;
	currentAddress = smack::Expr::id(BOOGIE_THIS);
	isGetter = false;
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

	if (isGetter && _node.arguments().size() > 0)
	{
		BOOST_THROW_EXCEPTION(InternalCompilerError() <<
						errinfo_comment("Getter should not have arguments") <<
						errinfo_sourceLocation(_node.location()));
	}

	// Get arguments recursively
	list<const smack::Expr*> args;
	// Pass extra arguments
	args.push_back(currentAddress); // this
	args.push_back(smack::Expr::id(BOOGIE_THIS)); // msg.sender
	// Add normal arguments
	for (auto arg : _node.arguments())
	{
		currentExpr = nullptr;
		arg->accept(*this);
		args.push_back(currentExpr);
	}

	// Assert is a separate statement in Boogie (instead of a function call)
	if (funcName == SOLIDITY_ASSERT)
	{
		// The parameter of assert is the first normal argument
		list<const smack::Expr*>::iterator it = args.begin();
		std::advance(it, 2);
		currentExpr = *it;
		currentBlocks.top()->addStmt(smack::Stmt::assert_(currentExpr));
		return false;
	}

	// TODO: check for void return in a more appropriate way
	if (_node.annotation().type->toString() != "tuple()")
	{
		// Create fresh variable to store the result
		smack::Decl* returnVar = smack::Decl::variable(
				funcName + "#" + to_string(_node.id()),
				mapType(_node.annotation().type, _node));
		localDecls.push_back(returnVar);
		// Result of the function call is the fresh variable
		currentExpr = smack::Expr::id(returnVar->getName());

		if (isGetter)
		{
			// Getters are replaced with map access (instead of function call)
			currentBlocks.top()->addStmt(smack::Stmt::assign(
					smack::Expr::id(returnVar->getName()),
					smack::Expr::sel(smack::Expr::id(funcName), currentAddress)));
		}
		else
		{
			list<string> rets;
			rets.push_back(returnVar->getName());
			// Assign call to the fresh variable
			currentBlocks.top()->addStmt(smack::Stmt::call(funcName, args, rets));
		}

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
	// Get expression recursively
	_node.expression().accept(*this);
	const smack::Expr* expr = currentExpr;
	// In case of a function call, the current expression is the address
	// on which the function is called
	currentAddress = currentExpr;
	// Type of the expression
	string typeStr = _node.expression().annotation().type->toString();

	// Handle special members/functions

	// address.balance
	if (typeStr == SOLIDITY_ADDRESS_TYPE && _node.memberName() == SOLIDITY_BALANCE)
	{
		currentExpr = smack::Expr::sel(smack::Expr::id(BOOGIE_BALANCE), expr);
	}
	// address.transfer()
	else if (typeStr == SOLIDITY_ADDRESS_TYPE && _node.memberName() == SOLIDITY_TRANSFER)
	{
		currentExpr = smack::Expr::id(BOOGIE_TRANSFER);
	}
	// msg.sender
	else if (typeStr == SOLIDITY_MSG && _node.memberName() == SOLIDITY_SENDER)
	{
		currentExpr = smack::Expr::id(BOOGIE_MSG_SENDER);
	}
	// Non-special member access
	else
	{
		currentExpr = smack::Expr::id(mapDeclName(*_node.annotation().referencedDeclaration));
		isGetter = false;
		if (dynamic_cast<const VariableDeclaration*>(_node.annotation().referencedDeclaration))
		{
			isGetter = true;
		}
	}
	return false;
}

bool ASTBoogieConverter::visit(IndexAccess const& _node)
{
	return visitNode(_node);
}

bool ASTBoogieConverter::visit(Identifier const& _node)
{
	string declName = mapDeclName(*(_node.annotation().referencedDeclaration));

	// Check if a state variable is referenced
	bool referencesStateVar = false;
	if (const VariableDeclaration* varDecl = dynamic_cast<const VariableDeclaration*>(_node.annotation().referencedDeclaration))
	{
		referencesStateVar = varDecl->isStateVariable();
	}

	// State variables must be referenced by accessing the map
	if (referencesStateVar) { currentExpr = smack::Expr::sel(declName, BOOGIE_THIS); }
	// Other identifiers can be referenced by their name
	else { currentExpr = smack::Expr::id(declName); }
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
