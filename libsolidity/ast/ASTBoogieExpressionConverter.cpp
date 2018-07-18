#include <algorithm>
#include <boost/algorithm/string/predicate.hpp>
#include <libsolidity/ast/ASTBoogieExpressionConverter.h>
#include <libsolidity/ast/ASTBoogieUtils.h>
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

const string ASTBoogieExpressionConverter::ERR_EXPR = "__ERROR";

const smack::Expr* ASTBoogieExpressionConverter::getArrayLength(const smack::Expr* expr, ASTNode const& associatedNode)
{
	// Array is a local variable
	if (auto varArray = dynamic_cast<const smack::VarExpr*>(expr))
	{
		return smack::Expr::id(varArray->name() + ASTBoogieUtils::BOOGIE_LENGTH);
	}
	// Array is state variable
	if (auto selArray = dynamic_cast<const smack::SelExpr*>(expr))
	{
		if (auto baseArray = dynamic_cast<const smack::VarExpr*>(selArray->getBase()))
		{
			return smack::Expr::sel(
							smack::Expr::id(baseArray->name() + ASTBoogieUtils::BOOGIE_LENGTH),
							selArray->getIdxs());
		}
	}

	m_errorReporter.error(Error::Type::ParserError, associatedNode.location(), "Array length access not supported by Boogie compiler");
	return smack::Expr::id(ERR_EXPR);
}

ASTBoogieExpressionConverter::ASTBoogieExpressionConverter(ErrorReporter& errorReporter, bool isInvariant) :
		m_errorReporter(errorReporter), m_isInvariant(isInvariant)
{
	m_currentExpr = nullptr;
	m_currentAddress = nullptr;
	m_isGetter = false;
	m_isLibraryCall = false;
	m_isLibraryCallStatic = false;
	m_currentValue = nullptr;
}

ASTBoogieExpressionConverter::Result ASTBoogieExpressionConverter::convert(const Expression& _node)
{
	m_currentExpr = nullptr;
	m_currentAddress = nullptr;
	m_isGetter = false;
	m_newStatements.clear();
	m_newDecls.clear();
	m_newConstants.clear();

	_node.accept(*this);

	return Result(m_currentExpr, m_newStatements, m_newDecls, m_newConstants);
}

bool ASTBoogieExpressionConverter::visit(Conditional const& _node)
{
	// Get condition recursively
	_node.condition().accept(*this);
	const smack::Expr* cond = m_currentExpr;

	// Get true expression recursively
	_node.trueExpression().accept(*this);
	const smack::Expr* trueExpr = m_currentExpr;

	// Get false expression recursively
	_node.falseExpression().accept(*this);
	const smack::Expr* falseExpr = m_currentExpr;

	m_currentExpr = smack::Expr::cond(cond, trueExpr, falseExpr);
	return false;
}

bool ASTBoogieExpressionConverter::visit(Assignment const& _node)
{
	// Get lhs recursively
	_node.leftHandSide().accept(*this);
	const smack::Expr* lhs = m_currentExpr;

	// Get rhs recursively
	_node.rightHandSide().accept(*this);
	const smack::Expr* rhs = m_currentExpr;

	// Result will be the LHS (for chained assignments like x = y = 5)
	m_currentExpr = lhs;

	// Transform rhs based on the operator, e.g., a += b becomes a := a + b
	switch (_node.assignmentOperator()) {
	case Token::Assign: break; // rhs already contains the result
	case Token::AssignAdd: rhs = smack::Expr::plus(lhs, rhs); break;
	case Token::AssignSub: rhs = smack::Expr::minus(lhs, rhs); break;
	case Token::AssignMul: rhs = smack::Expr::times(lhs, rhs); break;
	case Token::AssignDiv:
		if (ASTBoogieUtils::mapType(_node.annotation().type, _node, m_errorReporter) == "int") rhs = smack::Expr::intdiv(lhs, rhs);
		else rhs = smack::Expr::div(lhs, rhs);
		break;
	case Token::AssignMod: rhs = smack::Expr::mod(lhs, rhs); break;

	default:
		m_errorReporter.error(Error::Type::ParserError, _node.location(),
				string("Assignment operator ") + Token::toString(_node.assignmentOperator()) + " not supported by Boogie compiler");
		return false;
	}

	createAssignment(_node.leftHandSide(), lhs, rhs);

	return false;
}

void ASTBoogieExpressionConverter::createAssignment(Expression const& originalLhs, smack::Expr const *lhs, smack::Expr const* rhs)
{
	// If LHS is simply an identifier, we can assign to it
	if (dynamic_cast<smack::VarExpr const*>(lhs))
	{
		m_newStatements.push_back(smack::Stmt::assign(lhs, rhs));
		return;
	}

	// If LHS is an indexer (arrays/maps), it needs to be transformed to an update
	if (auto lhsSel = dynamic_cast<const smack::SelExpr*>(lhs))
	{
		if (auto lhsUpd = dynamic_cast<const smack::UpdExpr*>(selectToUpdate(lhsSel, rhs)))
		{
			m_newStatements.push_back(smack::Stmt::assign(lhsUpd->getBase(), lhsUpd));
			return;
		}
	}

	m_errorReporter.error(Error::Type::ParserError, originalLhs.location(), "Assignment not supported by Boogie compiler (LHS must be identifier/indexer)");
}

smack::Expr const* ASTBoogieExpressionConverter::selectToUpdate(smack::SelExpr const* sel, smack::Expr const* value)
{
	if (auto base = dynamic_cast<smack::SelExpr const*>(sel->getBase()))
	{
		return selectToUpdate(base, smack::Expr::upd(base, sel->getIdxs(), value));
	}
	else
	{
		return smack::Expr::upd(sel->getBase(), sel->getIdxs(), value);
	}
}


bool ASTBoogieExpressionConverter::visit(TupleExpression const& _node)
{
	if (_node.components().size() == 1)
	{
		_node.components()[0]->accept(*this);
	}
	else
	{
		m_errorReporter.error(Error::Type::ParserError, _node.location(), "Boogie compiler does not support tuples");
		m_currentExpr = smack::Expr::id(ERR_EXPR);
	}
	return false;
}

bool ASTBoogieExpressionConverter::visit(UnaryOperation const& _node)
{
	// Get operand recursively
	_node.subExpression().accept(*this);
	const smack::Expr* subExpr = m_currentExpr;

	switch (_node.getOperator()) {
	case Token::Not: m_currentExpr = smack::Expr::not_(subExpr); break;
	case Token::Sub: m_currentExpr = smack::Expr::neg(subExpr); break;
	case Token::Add: m_currentExpr = subExpr; break; // Unary plus does not do anything
	case Token::Inc:
		if (_node.isPrefixOperation()) // ++x
		{
			const smack::Expr* lhs = subExpr;
			const smack::Expr* rhs = smack::Expr::plus(lhs, smack::Expr::lit((unsigned)1));
			smack::Decl* tempVar = smack::Decl::variable("inc#" + to_string(_node.id()),
					ASTBoogieUtils::mapType(_node.subExpression().annotation().type, _node, m_errorReporter));
			m_newDecls.push_back(tempVar);
			// First do the assignment x := x + 1
			createAssignment(_node.subExpression(), lhs, rhs);
			// Then the assignment tmp := x
			m_newStatements.push_back(smack::Stmt::assign(smack::Expr::id(tempVar->getName()), lhs));
			// Result is the tmp variable (if the assignment is part of an expression)
			m_currentExpr = smack::Expr::id(tempVar->getName());
		}
		else // x++
		{
			const smack::Expr* lhs = subExpr;
			const smack::Expr* rhs = smack::Expr::plus(lhs, smack::Expr::lit((unsigned)1));
			smack::Decl* tempVar = smack::Decl::variable("inc#" + to_string(_node.id()),
							ASTBoogieUtils::mapType(_node.subExpression().annotation().type, _node, m_errorReporter));
			m_newDecls.push_back(tempVar);
			// First do the assignment tmp := x
			m_newStatements.push_back(smack::Stmt::assign(smack::Expr::id(tempVar->getName()), subExpr));
			// Then the assignment x := x + 1
			createAssignment(_node.subExpression(), lhs, rhs);
			// Result is the tmp variable (if the assignment is part of an expression)
			m_currentExpr = smack::Expr::id(tempVar->getName());
		}
		break;
	case Token::Dec:
		// Same as ++ but with minus
		if (_node.isPrefixOperation())
		{
			const smack::Expr* lhs = subExpr;
			const smack::Expr* rhs = smack::Expr::minus(lhs, smack::Expr::lit((unsigned)1));
			smack::Decl* tempVar = smack::Decl::variable("dec#" + to_string(_node.id()),
							ASTBoogieUtils::mapType(_node.subExpression().annotation().type, _node, m_errorReporter));
			m_newDecls.push_back(tempVar);
			createAssignment(_node.subExpression(), lhs, rhs);
			m_newStatements.push_back(smack::Stmt::assign(smack::Expr::id(tempVar->getName()), subExpr));
			m_currentExpr = smack::Expr::id(tempVar->getName());
		}
		else
		{
			const smack::Expr* lhs = subExpr;
			const smack::Expr* rhs = smack::Expr::minus(lhs, smack::Expr::lit((unsigned)1));
			smack::Decl* tempVar = smack::Decl::variable("dec#" + to_string(_node.id()),
							ASTBoogieUtils::mapType(_node.subExpression().annotation().type, _node, m_errorReporter));
			m_newDecls.push_back(tempVar);
			m_newStatements.push_back(smack::Stmt::assign(smack::Expr::id(tempVar->getName()), subExpr));
			createAssignment(_node.subExpression(), lhs, rhs);
			m_currentExpr = smack::Expr::id(tempVar->getName());
		}
		break;

	default:
		m_errorReporter.error(Error::Type::ParserError, _node.location(), string("Boogie compiler does not support unary operator ") + Token::toString(_node.getOperator()));
		m_currentExpr = smack::Expr::id(ERR_EXPR);
		break;
	}

	return false;
}

bool ASTBoogieExpressionConverter::visit(BinaryOperation const& _node)
{
	// Get lhs recursively
	_node.leftExpression().accept(*this);
	const smack::Expr* lhs = m_currentExpr;

	// Get rhs recursively
	_node.rightExpression().accept(*this);
	const smack::Expr* rhs = m_currentExpr;

	switch(_node.getOperator())
	{
	case Token::And: m_currentExpr = smack::Expr::and_(lhs, rhs); break;
	case Token::Or: m_currentExpr = smack::Expr::or_(lhs, rhs); break;

	case Token::Add: m_currentExpr = smack::Expr::plus(lhs, rhs); break;
	case Token::Sub: m_currentExpr = smack::Expr::minus(lhs, rhs); break;
	case Token::Mul: m_currentExpr = smack::Expr::times(lhs, rhs); break;
	case Token::Div: // Boogie has different division operators for integers and reals
		if (ASTBoogieUtils::mapType(_node.annotation().type, _node, m_errorReporter) == "int") m_currentExpr = smack::Expr::intdiv(lhs, rhs);
		else m_currentExpr = smack::Expr::div(lhs, rhs);
		break;
	case Token::Mod: m_currentExpr = smack::Expr::mod(lhs, rhs); break;

	case Token::Equal: m_currentExpr = smack::Expr::eq(lhs, rhs); break;
	case Token::NotEqual: m_currentExpr = smack::Expr::neq(lhs, rhs); break;
	case Token::LessThan: m_currentExpr = smack::Expr::lt(lhs, rhs); break;
	case Token::GreaterThan: m_currentExpr = smack::Expr::gt(lhs, rhs); break;
	case Token::LessThanOrEqual: m_currentExpr = smack::Expr::lte(lhs, rhs); break;
	case Token::GreaterThanOrEqual: m_currentExpr = smack::Expr::gte(lhs, rhs); break;

	case Token::Exp:
		if (auto rhsLit = dynamic_cast<smack::IntLit const *>(rhs))
		{
			if (auto lhsLit = dynamic_cast<smack::IntLit const *>(lhs))
			{
				m_currentExpr = smack::Expr::lit(boost::multiprecision::pow(lhsLit->getVal(), rhsLit->getVal().convert_to<unsigned>()));
				break;
			}
		}
		m_errorReporter.error(Error::Type::ParserError, _node.location(), "Exponentiation not supported by Boogie compiler");
		m_currentExpr = smack::Expr::id(ERR_EXPR);
		break;
	default:
		m_errorReporter.error(Error::Type::ParserError, _node.location(), string("Boogie compiler does not support binary operator ") + Token::toString(_node.getOperator()));
		m_currentExpr = smack::Expr::id(ERR_EXPR);
		break;
	}

	return false;
}

bool ASTBoogieExpressionConverter::visit(FunctionCall const& _node)
{
	// Check for conversions
	if (_node.annotation().kind == FunctionCallKind::TypeConversion)
	{
		// Nothing to do when converting to address
		if (auto expr = dynamic_cast<ElementaryTypeNameExpression const*>(&_node.expression()))
		{
			if (expr->typeName().toString() == ASTBoogieUtils::SOLIDITY_ADDRESS_TYPE)
			{
				(*_node.arguments().begin())->accept(*this);
				return false;
			}
		}
		// Nothing to do when converting to other kind of contract
		if (auto expr = dynamic_cast<Identifier const*>(&_node.expression()))
		{
			if (dynamic_cast<ContractDefinition const *>(expr->annotation().referencedDeclaration))
			{
				(*_node.arguments().begin())->accept(*this);
				return false;
			}
		}
		// Nothing to do when the two types are mapped to same type in Boogie,
		// e.g., conversion from uint256 to int256 if both are mapped to int
		if (ASTBoogieUtils::mapType(_node.annotation().type, _node, m_errorReporter) ==
			ASTBoogieUtils::mapType((*_node.arguments().begin())->annotation().type, **_node.arguments().begin(), m_errorReporter))
		{
			(*_node.arguments().begin())->accept(*this);
			return false;
		}

		m_errorReporter.error(Error::Type::ParserError, _node.location(), "Type conversion not supported by Boogie compiler");
		m_currentExpr = smack::Expr::id(ERR_EXPR);
		return false;

	}
	// Function calls in Boogie are statements and cannot be part of
	// expressions, therefore each function call is given a fresh variable
	// for its return value and is mapped to a call statement

	// First, process the expression of the function call, which should give:
	// - The name of the called function in 'currentExpr'
	// - The address on which the function is called in 'currentAddress'
	// - The msg.value in 'currentValue'
	// Example: f(z) gives 'f' as the name and 'this' as the address
	// Example: x.f(z) gives 'f' as the name and 'x' as the address
	// Example: x.f.value(y)(z) gives 'f' as the name, 'x' as the address and 'y' as the value

	// Check for the special case of calling the 'value' function
	// For example x.f.value(y)(z) should be treated as x.f(z), while setting
	// 'currentValue' to 'y'.
	if (auto expMa = dynamic_cast<const MemberAccess*>(&_node.expression()))
	{
		if (expMa->memberName() == "value")
		{
			// Process the argument
			if (_node.arguments().size() != 1)
			{
				BOOST_THROW_EXCEPTION(InternalCompilerError() <<
								errinfo_comment("Call to the value function should have exactly one argument") <<
								errinfo_sourceLocation(_node.location()));
			}
			(*_node.arguments().begin())->accept(*this);
			m_currentValue = m_currentExpr;

			// Continue with the rest of the AST
			expMa->expression().accept(*this);
			return false;
		}
	}

	// Ignore gas setting, e.g., x.f.gas(y)(z) is just x.f(z)
	if (auto expMa = dynamic_cast<const MemberAccess*>(&_node.expression()))
	{
		if (expMa->memberName() == "gas")
		{
			m_errorReporter.warning(expMa->location(), "Ignored call to gas() function.");
			expMa->expression().accept(*this);
			return false;
		}
	}

	m_currentExpr = nullptr;
	m_currentAddress = smack::Expr::id(ASTBoogieUtils::BOOGIE_THIS);
	m_currentValue = smack::Expr::lit((long)0);
	m_isGetter = false;
	m_isLibraryCall = false;
	_node.expression().accept(*this);

	// 'currentExpr' should be an identifier, giving the name of the function
	string funcName;
	if (auto v = dynamic_cast<smack::VarExpr const*>(m_currentExpr))
	{
		funcName = v->name();
	}
	else
	{
		m_errorReporter.error(Error::Type::ParserError, _node.location(), "Boogie compiler only supports identifiers as function calls");
		funcName = ERR_EXPR;
	}

	if (m_isGetter && _node.arguments().size() > 0)
	{
		m_errorReporter.error(Error::Type::ParserError, _node.location(), "Boogie compiler does not support getter arguments");
	}

	// Get arguments recursively
	list<const smack::Expr*> args;
	// Pass extra arguments
	if (!m_isLibraryCall) { args.push_back(m_currentAddress); } // this
	args.push_back(smack::Expr::id(ASTBoogieUtils::BOOGIE_THIS)); // msg.sender
	args.push_back(m_currentValue); // msg.value
	if (m_isLibraryCall && !m_isLibraryCallStatic) { args.push_back(m_currentAddress); } // Non-static library calls require extra argument
	// Add normal arguments (except when calling 'call') TODO: is this ok?
	if (funcName != ASTBoogieUtils::BOOGIE_CALL)
	{
		for (auto arg : _node.arguments())
		{
			arg->accept(*this);
			args.push_back(m_currentExpr);

			if (arg->annotation().type && arg->annotation().type->category() == Type::Category::Array)
			{
				args.push_back(getArrayLength(m_currentExpr, *arg));
			}
		}
	}

	// Assert is a separate statement in Boogie (instead of a function call)
	if (funcName == ASTBoogieUtils::SOLIDITY_ASSERT)
	{
		if (_node.arguments().size() != 1)
		{
			BOOST_THROW_EXCEPTION(InternalCompilerError() <<
							errinfo_comment("Assert should have exactly one argument") <<
							errinfo_sourceLocation(_node.location()));
		}
		// The parameter of assert is the first (and only) normal argument
		list<const smack::Expr*>::iterator it = args.begin();
		std::advance(it, args.size() - _node.arguments().size());
		m_newStatements.push_back(smack::Stmt::assert_(*it));
		return false;
	}

	// Require is mapped to assume statement in Boogie (instead of a function call)
	if (funcName == ASTBoogieUtils::SOLIDITY_REQUIRE)
	{
		if (2 < _node.arguments().size() || _node.arguments().size() < 1)
		{
			BOOST_THROW_EXCEPTION(InternalCompilerError() <<
							errinfo_comment("Require should have one or two argument(s)") <<
							errinfo_sourceLocation(_node.location()));
		}
		// The parameter of assume is the first normal argument (the second is
		// an message optional message that is omitted in Boogie)
		list<const smack::Expr*>::iterator it = args.begin();
		std::advance(it, args.size() - _node.arguments().size());
		m_newStatements.push_back(smack::Stmt::assume(*it));
		return false;
	}

	// Revert is mapped to assume(false) statement in Boogie (instead of a function call)
	// Its argument is an optional message that is omitted in Boogie
	if (funcName == ASTBoogieUtils::SOLIDITY_REVERT)
	{
		if (_node.arguments().size() > 1)
		{
			BOOST_THROW_EXCEPTION(InternalCompilerError() <<
							errinfo_comment("Revert should have at most one argument") <<
							errinfo_sourceLocation(_node.location()));
		}
		m_newStatements.push_back(smack::Stmt::assume(smack::Expr::lit(false)));
		return false;
	}

	if (m_isInvariant && funcName == ASTBoogieUtils::VERIFIER_SUM)
	{
		if (auto sumBase = dynamic_cast<Identifier const*>(&*_node.arguments()[0]))
		{
			m_currentExpr = smack::Expr::sel(smack::Expr::id(sumBase->name() + "#sum"), smack::Expr::id(ASTBoogieUtils::BOOGIE_THIS));
			return false;
		}

	}

	// TODO: check for void return in a more appropriate way
	if (_node.annotation().type->toString() != "tuple()")
	{
		// Create fresh variable to store the result
		smack::Decl* returnVar = smack::Decl::variable(
				funcName + "#" + to_string(_node.id()),
				ASTBoogieUtils::mapType(_node.annotation().type, _node, m_errorReporter));
		m_newDecls.push_back(returnVar);
		// Result of the function call is the fresh variable
		m_currentExpr = smack::Expr::id(returnVar->getName());

		if (m_isGetter)
		{
			// Getters are replaced with map access (instead of function call)
			m_newStatements.push_back(smack::Stmt::assign(
					smack::Expr::id(returnVar->getName()),
					smack::Expr::sel(smack::Expr::id(funcName), m_currentAddress)));
		}
		else
		{
			list<string> rets;
			rets.push_back(returnVar->getName());
			// Assign call to the fresh variable
			m_newStatements.push_back(smack::Stmt::call(funcName, args, rets));
		}

	}
	else
	{
		m_currentExpr = nullptr; // No return value for function
		m_newStatements.push_back(smack::Stmt::call(funcName, args));
	}

	return false;
}

bool ASTBoogieExpressionConverter::visit(NewExpression const& _node)
{
	m_errorReporter.error(Error::Type::ParserError, _node.location(), "Boogie compiler does not support new expression");
	m_currentExpr = smack::Expr::id(ERR_EXPR);
	return false;
}

bool ASTBoogieExpressionConverter::visit(MemberAccess const& _node)
{
	// Normally, the expression of the MemberAccess will give the address and
	// the memberName will give the name. For example, x.f() will have address
	// 'x' and name 'f'.

	// Get expression recursively
	_node.expression().accept(*this);
	const smack::Expr* expr = m_currentExpr;
	// The current expression gives the address on which something is done
	m_currentAddress = m_currentExpr;
	// Type of the expression
	string typeStr = _node.expression().annotation().type->toString();

	// Handle special members/functions

	// address.balance / this.balance
	if (_node.memberName() == ASTBoogieUtils::SOLIDITY_BALANCE)
	{
		if (typeStr == ASTBoogieUtils::SOLIDITY_ADDRESS_TYPE)
		{
			m_currentExpr = smack::Expr::sel(smack::Expr::id(ASTBoogieUtils::BOOGIE_BALANCE), expr);
			return false;
		}
		if (auto exprId = dynamic_cast<Identifier const*>(&_node.expression()))
		{
			if (exprId->name() == ASTBoogieUtils::SOLIDITY_THIS)
			{
				m_currentExpr = smack::Expr::sel(smack::Expr::id(ASTBoogieUtils::BOOGIE_BALANCE), smack::Expr::id(ASTBoogieUtils::BOOGIE_THIS));
				return false;
			}
		}
	}
	// address.transfer()
	if (typeStr == ASTBoogieUtils::SOLIDITY_ADDRESS_TYPE && _node.memberName() == ASTBoogieUtils::SOLIDITY_TRANSFER)
	{
		m_currentExpr = smack::Expr::id(ASTBoogieUtils::BOOGIE_TRANSFER);
		return false;
	}
	// address.send()
	if (typeStr == ASTBoogieUtils::SOLIDITY_ADDRESS_TYPE && _node.memberName() == ASTBoogieUtils::SOLIDITY_SEND)
	{
		m_currentExpr = smack::Expr::id(ASTBoogieUtils::BOOGIE_SEND);
		return false;
	}
	// address.call()
	if (typeStr == ASTBoogieUtils::SOLIDITY_ADDRESS_TYPE && _node.memberName() == ASTBoogieUtils::SOLIDITY_CALL)
	{
		m_currentExpr = smack::Expr::id(ASTBoogieUtils::BOOGIE_CALL);
		return false;
	}
	// msg.sender
	if (typeStr == ASTBoogieUtils::SOLIDITY_MSG && _node.memberName() == ASTBoogieUtils::SOLIDITY_SENDER)
	{
		m_currentExpr = smack::Expr::id(ASTBoogieUtils::BOOGIE_MSG_SENDER);
		return false;
	}
	// msg.value
	if (typeStr == ASTBoogieUtils::SOLIDITY_MSG && _node.memberName() == ASTBoogieUtils::SOLIDITY_VALUE)
	{
		m_currentExpr = smack::Expr::id(ASTBoogieUtils::BOOGIE_MSG_VALUE);
		return false;
	}
	// array.length
	if (_node.expression().annotation().type->category() == Type::Category::Array && _node.memberName() == "length")
	{
		m_currentExpr = getArrayLength(expr, _node);
		return false;
	}
	// fixed size byte array length
	if (_node.expression().annotation().type->category() == Type::Category::FixedBytes && _node.memberName() == "length")
	{
		auto fbType = dynamic_cast<FixedBytesType const*>(&*_node.expression().annotation().type);
		m_currentExpr = smack::Expr::lit(fbType->numBytes());
		return false;
	}
	// Non-special member access: 'referencedDeclaration' should point to the
	// declaration corresponding to 'memberName'
	if (_node.annotation().referencedDeclaration == nullptr)
	{
		m_errorReporter.error(Error::Type::ParserError, _node.location(), "Boogie compiler does not support member without corresponding declaration (probably an unsupported special member)");
		m_currentExpr = smack::Expr::id(ERR_EXPR);
		return false;
	}
	m_currentExpr = smack::Expr::id(ASTBoogieUtils::mapDeclName(*_node.annotation().referencedDeclaration));
	// Check for getter
	m_isGetter = false;
	if (dynamic_cast<const VariableDeclaration*>(_node.annotation().referencedDeclaration))
	{
		m_isGetter = true;
	}
	// Check for library call
	m_isLibraryCall = false;
	if (auto fDef = dynamic_cast<const FunctionDefinition*>(_node.annotation().referencedDeclaration))
	{
		m_isLibraryCall = fDef->inContractKind() == ContractDefinition::ContractKind::Library;
		if (m_isLibraryCall)
		{
			// Check if library call is static (e.g., Math.add(1, 2)) or not (e.g., 1.add(2))
			m_isLibraryCallStatic = false;
			if (auto exprId = dynamic_cast<Identifier const *>(&_node.expression()))
			{
				if (dynamic_cast<ContractDefinition const *>(exprId->annotation().referencedDeclaration))
				{
					m_isLibraryCallStatic = true;
				}
			}
		}
	}

	return false;
}

bool ASTBoogieExpressionConverter::visit(IndexAccess const& _node)
{
	_node.baseExpression().accept(*this);
	const smack::Expr* baseExpr = m_currentExpr;

	_node.indexExpression()->accept(*this); // TODO: can this be a nullptr?
	const smack::Expr* indexExpr = m_currentExpr;

	// The type bytes1 is represented as a scalar value in Boogie, therefore
	// indexing is not required, but an assertion is added to check the index
	if (_node.baseExpression().annotation().type->category() == Type::Category::FixedBytes)
	{
		auto fbType = dynamic_cast<FixedBytesType const*>(&*_node.baseExpression().annotation().type);
		if (fbType->numBytes() == 1)
		{
			m_newStatements.push_back(smack::Stmt::assert_(smack::Expr::eq(indexExpr, smack::Expr::lit((unsigned)0))));
			m_currentExpr = baseExpr;
			return false;
		}
	}
	// Index access is simply converted to a select in Boogie, which is fine
	// as long as it is not an LHS of an assignment (e.g., x[i] = v), but
	// that case is handled when converting assignments
	m_currentExpr = smack::Expr::sel(baseExpr, indexExpr);

	return false;
}

bool ASTBoogieExpressionConverter::visit(Identifier const& _node)
{
	if (_node.name() == ASTBoogieUtils::VERIFIER_SUM)
	{
		m_currentExpr = smack::Expr::id(ASTBoogieUtils::VERIFIER_SUM);
		return false;
	}

	if (!_node.annotation().referencedDeclaration)
	{
		m_errorReporter.error(Error::Type::ParserError, _node.location(), "Boogie compiler found an identifier without matching declaration");
		m_currentExpr = smack::Expr::id(ERR_EXPR);
		return false;
	}
	string declName = ASTBoogieUtils::mapDeclName(*(_node.annotation().referencedDeclaration));

	// Check if a state variable is referenced
	bool referencesStateVar = false;
	if (auto varDecl = dynamic_cast<const VariableDeclaration*>(_node.annotation().referencedDeclaration))
	{
		referencesStateVar = varDecl->isStateVariable();
	}

	// State variables must be referenced by accessing the map
	if (referencesStateVar) { m_currentExpr = smack::Expr::sel(declName, ASTBoogieUtils::BOOGIE_THIS); }
	// Other identifiers can be referenced by their name
	else { m_currentExpr = smack::Expr::id(declName); }
	return false;
}

bool ASTBoogieExpressionConverter::visit(ElementaryTypeNameExpression const& _node)
{
	BOOST_THROW_EXCEPTION(InternalCompilerError() << errinfo_comment("Unhandled node: ElementaryTypeNameExpression") << errinfo_sourceLocation(_node.location()));
	return false;
}

bool ASTBoogieExpressionConverter::visit(Literal const& _node)
{
	if (m_isInvariant && !_node.annotation().type)
	{

	}

	string tpStr = _node.annotation().type->toString();
	// TODO: option for bit precise types
	if (boost::starts_with(tpStr, "int_const"))
	{
		m_currentExpr = smack::Expr::lit(smack::bigint(_node.value()));
		return false;
	}
	if (tpStr == "bool")
	{
		m_currentExpr = smack::Expr::lit(_node.value() == "true");
		return false;
	}
	if (tpStr == ASTBoogieUtils::SOLIDITY_ADDRESS_TYPE)
	{
		string name = "address_" + _node.value();
		m_newConstants.push_back(smack::Decl::constant(name, ASTBoogieUtils::BOOGIE_ADDRESS_TYPE, true));
		m_currentExpr = smack::Expr::id(name);
		return false;
	}
	if (boost::starts_with(tpStr, "literal_string"))
	{
		string name = "literal_string#" + to_string(_node.id());
		m_newConstants.push_back(smack::Decl::constant(name, ASTBoogieUtils::BOOGIE_STRING_TYPE, true));
		m_currentExpr = smack::Expr::id(name);
		return false;
	}

	m_errorReporter.error(Error::Type::ParserError, _node.location(), "Boogie compiler does not support literals for type " + tpStr.substr(0, tpStr.find(' ')));
	m_currentExpr = smack::Expr::id(ERR_EXPR);
	return false;
}

bool ASTBoogieExpressionConverter::visitNode(ASTNode const& _node)
{
	BOOST_THROW_EXCEPTION(InternalCompilerError() <<
					errinfo_comment("Unhandled node") <<
					errinfo_sourceLocation(_node.location()));
	return true;
}

}
}
