#include <algorithm>
#include <boost/algorithm/string/predicate.hpp>
#include <libsolidity/ast/ASTBoogieExpressionConverter.h>
#include <libsolidity/ast/ASTBoogieUtils.h>
#include <libsolidity/ast/BoogieAst.h>
#include <libsolidity/interface/Exceptions.h>
#include <libsolidity/parsing/Scanner.h>
#include <utility>
#include <tuple>

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

	reportError(associatedNode.location(), "Unsupported access to array length");
	return smack::Expr::id(ERR_EXPR);
}

const smack::Expr* ASTBoogieExpressionConverter::getSumShadowVar(ASTNode const* node)
{
	if (auto sumBase = dynamic_cast<Identifier const*>(node))
	{
		auto sumBaseDecl = sumBase->annotation().referencedDeclaration;
		if (sumBaseDecl != nullptr)
		{
			return smack::Expr::sel(
					smack::Expr::id(ASTBoogieUtils::mapDeclName(*sumBaseDecl) + ASTBoogieUtils::BOOGIE_SUM),
					smack::Expr::id(ASTBoogieUtils::BOOGIE_THIS));
		}
	}
	reportError(node->location(), "Unsupported identifier for sum function");
	return smack::Expr::id(ERR_EXPR);
}


const smack::Expr* ASTBoogieExpressionConverter::bvBinaryFunc(ASTNode const& associatedNode, Token::Value op,
		smack::Expr const* lhs, smack::Expr const* rhs, unsigned bits, bool isSigned)
{
	string uns("");
	string name("");
	string retType("");
	switch (op) {
	case Token::Add: name = "add"; retType = "bv" + to_string(bits); break;
	case Token::Sub: name = "sub"; retType = "bv" + to_string(bits); break;
	case Token::Mul: name = "mul"; retType = "bv" + to_string(bits); break;
	case Token::Div: name = isSigned ? "sdiv" : "udiv"; retType = "bv" + to_string(bits); break;

	case Token::BitAnd: name = "and"; retType = "bv" + to_string(bits); break;
	case Token::BitOr: name = "or"; retType = "bv" + to_string(bits); break;
	case Token::BitXor: name = "xor"; retType = "bv" + to_string(bits); break;

	case Token::Equal: return smack::Expr::eq(lhs, rhs);
	case Token::NotEqual: return smack::Expr::neq(lhs, rhs);

	case Token::LessThan: name = isSigned ? "slt" : "ult"; retType = "bool"; break;
	case Token::GreaterThan: name = isSigned ? "sgt" : "ugt"; retType = "bool";  break;
	case Token::LessThanOrEqual: name = isSigned ? "sle" : "ule"; retType = "bool";  break;
	case Token::GreaterThanOrEqual: name = isSigned ? "sge" : "uge"; retType = "bool";  break;
	default:
		reportError(associatedNode.location(), string("Unsupported binary operator in bit-precise mode ") + Token::toString(op));
		return smack::Expr::id(ERR_EXPR);
	}
	string fullName = "bv" + to_string(bits) + name;
	// TODO: check if exists
	m_context.bvBuiltinFunctions()[fullName] = smack::Decl::function(
					fullName, {make_pair("", "bv"+to_string(bits)), make_pair("", "bv"+to_string(bits))}, retType, nullptr,
					{smack::Attr::attr("bvbuiltin", "bv" + name)});

	return smack::Expr::fn("bv" + to_string(bits) + name, lhs, rhs);
}

const smack::Expr* ASTBoogieExpressionConverter::bvUnaryFunc(ASTNode const& associatedNode, Token::Value op,
		smack::Expr const* subExpr, unsigned bits, bool)
{
	string name("");
	switch (op) {
	case Token::Add: return subExpr;
	case Token::Sub: name = "neg"; break;
	case Token::BitNot: name = "not"; break;
	default:
		reportError(associatedNode.location(), string("Unsupported unary operator in bit-precise mode ") + Token::toString(op));
		return smack::Expr::id(ERR_EXPR);
	}

	string fullName = "bv" + to_string(bits) + name;
	// TODO: check if exists
	m_context.bvBuiltinFunctions()[fullName] = smack::Decl::function(
					fullName, {make_pair("", "bv"+to_string(bits))}, "bv"+to_string(bits), nullptr,
					{smack::Attr::attr("bvbuiltin", "bv" + name)});

	return smack::Expr::fn("bv" + to_string(bits) + name, subExpr);
}

void ASTBoogieExpressionConverter::reportError(SourceLocation const& location, std::string const& description)
{
	m_context.errorReporter().error(Error::Type::ParserError, m_defaultLocation ? *m_defaultLocation : location, description);
}

void ASTBoogieExpressionConverter::reportWarning(SourceLocation const& location, std::string const& description)
{
	m_context.errorReporter().warning(m_defaultLocation ? *m_defaultLocation : location, description);
}

ASTBoogieExpressionConverter::ASTBoogieExpressionConverter(BoogieContext& context, SourceLocation const* defaultLocation) :
		m_context(context), m_defaultLocation(defaultLocation)
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
	m_isLibraryCall = false;
	m_isLibraryCallStatic = false;
	m_currentValue = nullptr;
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

	if (m_context.bitPrecise() && ASTBoogieUtils::isBitPreciseType(_node.leftHandSide().annotation().type))
	{
		unsigned bits = ASTBoogieUtils::getBits(_node.leftHandSide().annotation().type);
		bool isSigned = ASTBoogieUtils::isSigned(_node.leftHandSide().annotation().type);
		rhs = ASTBoogieUtils::checkImplicitBvConversion(rhs, _node.rightHandSide().annotation().type, _node.leftHandSide().annotation().type, m_context.bvBuiltinFunctions());

		switch(_node.assignmentOperator())
		{
		case Token::Assign: break; // rhs already contains the result
		case Token::AssignAdd: rhs = bvBinaryFunc(_node, Token::Add, lhs, rhs, bits, isSigned); break;
		case Token::AssignSub: rhs = bvBinaryFunc(_node, Token::Sub, lhs, rhs, bits, isSigned); break;
		case Token::AssignMul: rhs = bvBinaryFunc(_node, Token::Mul, lhs, rhs, bits, isSigned); break;
		case Token::AssignDiv: rhs = bvBinaryFunc(_node, Token::Div, lhs, rhs, bits, isSigned); break;
		case Token::AssignBitAnd: rhs = bvBinaryFunc(_node, Token::BitAnd, lhs, rhs, bits, isSigned); break;
		case Token::AssignBitOr: rhs = bvBinaryFunc(_node, Token::BitOr, lhs, rhs, bits, isSigned); break;
		case Token::AssignBitXor: rhs = bvBinaryFunc(_node, Token::BitXor, lhs, rhs, bits, isSigned); break;
		default:
			reportError(_node.location(), string("Unsupported assignment operator in bit-precise mode: ") +
					Token::toString(_node.assignmentOperator()));
			return false;
		}
	}
	else
	{
		// Transform rhs based on the operator, e.g., a += b becomes a := a + b
		switch (_node.assignmentOperator()) {
		case Token::Assign: break; // rhs already contains the result
		case Token::AssignAdd: rhs = smack::Expr::plus(lhs, rhs); break;
		case Token::AssignSub: rhs = smack::Expr::minus(lhs, rhs); break;
		case Token::AssignMul: rhs = smack::Expr::times(lhs, rhs); break;
		case Token::AssignDiv:
			if (ASTBoogieUtils::mapType(_node.annotation().type, _node, m_context.errorReporter(), m_context.bitPrecise()) == "int") rhs = smack::Expr::intdiv(lhs, rhs);
			else rhs = smack::Expr::div(lhs, rhs);
			break;
		case Token::AssignMod: rhs = smack::Expr::mod(lhs, rhs); break;
		case Token::AssignBitAnd:
		case Token::AssignBitOr:
		case Token::AssignBitXor:
			reportError(_node.location(), string("Use bit-precise mode for bitwise operator ") + Token::toString(_node.assignmentOperator()));
			m_currentExpr = smack::Expr::id(ERR_EXPR);
			break;

		default:
			reportError(_node.location(), string("Unsupported assignment operator: ") +
					Token::toString(_node.assignmentOperator()));
			return false;
		}
	}

	createAssignment(_node.leftHandSide(), lhs, rhs);

	return false;
}

void ASTBoogieExpressionConverter::createAssignment(Expression const& originalLhs, smack::Expr const *lhs, smack::Expr const* rhs)
{
	// First check if shadow variables need to be updated
	if (auto lhsIdx = dynamic_cast<IndexAccess const*>(&originalLhs))
	{
		if (auto lhsId = dynamic_cast<Identifier const*>(&lhsIdx->baseExpression()))
		{
			if (find(m_context.currentSumDecls().begin(), m_context.currentSumDecls().end(), lhsId->annotation().referencedDeclaration) != m_context.currentSumDecls().end())
			{
				// arr[i] = x becomes arr#sum := arr#sum[this := (arr#sum[this] + x - arr[i])]
				auto sumId = smack::Expr::id(ASTBoogieUtils::mapDeclName(*lhsId->annotation().referencedDeclaration) + ASTBoogieUtils::BOOGIE_SUM);
				smack::Expr const* upd = nullptr;
				if (m_context.bitPrecise())
				{
					unsigned bits = ASTBoogieUtils::getBits(originalLhs.annotation().type);
					bool isSigned = ASTBoogieUtils::isSigned(originalLhs.annotation().type);
					upd = bvBinaryFunc(originalLhs, Token::Value::Add, smack::Expr::sel(sumId, smack::Expr::id(ASTBoogieUtils::BOOGIE_THIS)),
							bvBinaryFunc(originalLhs, Token::Value::Sub, rhs, lhs, bits, isSigned), bits, isSigned);
				}
				else
				{
					upd = smack::Expr::plus(smack::Expr::sel(sumId, smack::Expr::id(ASTBoogieUtils::BOOGIE_THIS)), smack::Expr::minus(rhs, lhs));
				}

				m_newStatements.push_back(smack::Stmt::assign(
						sumId,
						smack::Expr::upd(sumId, smack::Expr::id(ASTBoogieUtils::BOOGIE_THIS), upd)));
			}
		}
	}


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

	reportError(originalLhs.location(), "Unsupported assignment (LHS must be identifier/indexer)");
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
		reportError(_node.location(), "Tuples are not supported");
		m_currentExpr = smack::Expr::id(ERR_EXPR);
	}
	return false;
}

bool ASTBoogieExpressionConverter::visit(UnaryOperation const& _node)
{
	string tpStr = _node.annotation().type->toString();
	if (boost::starts_with(tpStr, "int_const"))
	{
		m_currentExpr = smack::Expr::lit(smack::bigint(tpStr.substr(10)));
		return false;
	}

	// Get operand recursively
	_node.subExpression().accept(*this);
	const smack::Expr* subExpr = m_currentExpr;

	if (m_context.bitPrecise() && ASTBoogieUtils::isBitPreciseType(_node.annotation().type))
	{
		unsigned bits = ASTBoogieUtils::getBits(_node.annotation().type);

		switch (_node.getOperator()) {
		case Token::Add:
		case Token::Sub:
		case Token::BitNot:
			m_currentExpr = bvUnaryFunc(_node, _node.getOperator(), subExpr, bits);
			break;

		// Inc and Dec shares most part of the code
		case Token::Inc:
		case Token::Dec:
			{
				const smack::Expr* lhs = subExpr;
				const smack::Expr* rhs = _node.getOperator() == Token::Inc ?
						bvBinaryFunc(_node, Token::Add, lhs, smack::Expr::lit(smack::bigint(1), bits), bits) :
						bvBinaryFunc(_node, Token::Sub, lhs, smack::Expr::lit(smack::bigint(1), bits), bits);
				smack::Decl* tempVar = smack::Decl::variable("inc#" + to_string(_node.id()),
						ASTBoogieUtils::mapType(_node.subExpression().annotation().type, _node, m_context.errorReporter(), m_context.bitPrecise()));
				m_newDecls.push_back(tempVar);
				if (_node.isPrefixOperation()) // ++x (or --x)
				{
					// First do the assignment x := x + 1 (or x := x - 1)
					createAssignment(_node.subExpression(), lhs, rhs);
					// Then the assignment tmp := x
					m_newStatements.push_back(smack::Stmt::assign(smack::Expr::id(tempVar->getName()), lhs));
				}
				else // x++ (or x--)
				{
					// First do the assignment tmp := x
					m_newStatements.push_back(smack::Stmt::assign(smack::Expr::id(tempVar->getName()), subExpr));
					// Then the assignment x := x + 1 (or x := x - 1)
					createAssignment(_node.subExpression(), lhs, rhs);
				}
				// Result is the tmp variable (if the assignment is part of an expression)
				m_currentExpr = smack::Expr::id(tempVar->getName());
			}
			break;
		default:
			reportError(_node.location(), string("Unsupported unary operator in bit-precise mode: ") + Token::toString(_node.getOperator()));
			m_currentExpr = smack::Expr::id(ERR_EXPR);
			break;
		}
	}
	else
	{
		switch (_node.getOperator()) {
		case Token::Add: m_currentExpr = subExpr; break; // Unary plus does not do anything
		case Token::Not: m_currentExpr = smack::Expr::not_(subExpr); break;
		case Token::Sub: m_currentExpr = smack::Expr::neg(subExpr); break;
		// Inc and Dec shares most part of the code
		case Token::Inc:
		case Token::Dec:
			{
				const smack::Expr* lhs = subExpr;
				const smack::Expr* rhs = _node.getOperator() == Token::Inc ?
						smack::Expr::plus(lhs, smack::Expr::lit((unsigned)1)) :
						smack::Expr::minus(lhs, smack::Expr::lit((unsigned)1));
				smack::Decl* tempVar = smack::Decl::variable("inc#" + to_string(_node.id()),
						ASTBoogieUtils::mapType(_node.subExpression().annotation().type, _node, m_context.errorReporter(), m_context.bitPrecise()));
				m_newDecls.push_back(tempVar);
				if (_node.isPrefixOperation()) // ++x (or --x)
				{
					// First do the assignment x := x + 1 (or x := x - 1)
					createAssignment(_node.subExpression(), lhs, rhs);
					// Then the assignment tmp := x
					m_newStatements.push_back(smack::Stmt::assign(smack::Expr::id(tempVar->getName()), lhs));
				}
				else // x++ (or x--)
				{
					// First do the assignment tmp := x
					m_newStatements.push_back(smack::Stmt::assign(smack::Expr::id(tempVar->getName()), subExpr));
					// Then the assignment x := x + 1 (or x := x - 1)
					createAssignment(_node.subExpression(), lhs, rhs);
				}
				// Result is the tmp variable (if the assignment is part of an expression)
				m_currentExpr = smack::Expr::id(tempVar->getName());
			}
			break;
		default:
			reportError(_node.location(), string("Unsupported unary operator: ") + Token::toString(_node.getOperator()));
			m_currentExpr = smack::Expr::id(ERR_EXPR);
			break;
		}
	}

	return false;
}

bool ASTBoogieExpressionConverter::visit(BinaryOperation const& _node)
{
	string tpStr = _node.annotation().type->toString();
	if (boost::starts_with(tpStr, "int_const"))
	{
		m_currentExpr = smack::Expr::lit(smack::bigint(tpStr.substr(10)));
		return false;
	}

	// Get lhs recursively
	_node.leftExpression().accept(*this);
	const smack::Expr* lhs = m_currentExpr;

	// Get rhs recursively
	_node.rightExpression().accept(*this);
	const smack::Expr* rhs = m_currentExpr;

	auto commonType = _node.leftExpression().annotation().type->binaryOperatorResult(_node.getOperator(), _node.rightExpression().annotation().type);

	if (m_context.bitPrecise() && ASTBoogieUtils::isBitPreciseType(commonType))
	{
		lhs = ASTBoogieUtils::checkImplicitBvConversion(lhs, _node.leftExpression().annotation().type, commonType, m_context.bvBuiltinFunctions());
		rhs = ASTBoogieUtils::checkImplicitBvConversion(rhs, _node.rightExpression().annotation().type, commonType, m_context.bvBuiltinFunctions());

		m_currentExpr = bvBinaryFunc(_node, _node.getOperator(), lhs, rhs, ASTBoogieUtils::getBits(commonType), ASTBoogieUtils::isSigned(commonType));
	}
	else
	{
		switch(_node.getOperator())
		{
		case Token::And: m_currentExpr = smack::Expr::and_(lhs, rhs); break;
		case Token::Or: m_currentExpr = smack::Expr::or_(lhs, rhs); break;

		case Token::Add: m_currentExpr = smack::Expr::plus(lhs, rhs); break;
		case Token::Sub: m_currentExpr = smack::Expr::minus(lhs, rhs); break;
		case Token::Mul: m_currentExpr = smack::Expr::times(lhs, rhs); break;
		case Token::Div: // Boogie has different division operators for integers and reals
			if (ASTBoogieUtils::mapType(_node.annotation().type, _node, m_context.errorReporter(), m_context.bitPrecise()) == "int") m_currentExpr = smack::Expr::intdiv(lhs, rhs);
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
			reportError(_node.location(), "Exponentiation is not supported");
			m_currentExpr = smack::Expr::id(ERR_EXPR);
			break;
		case Token::BitAnd:
		case Token::BitOr:
		case Token::BitXor:
			reportError(_node.location(), string("Use bit-precise mode for bitwise operator ") + Token::toString(_node.getOperator()));
			m_currentExpr = smack::Expr::id(ERR_EXPR);
			break;
		default:
			reportError(_node.location(), string("Unsupported binary operator ") + Token::toString(_node.getOperator()));
			m_currentExpr = smack::Expr::id(ERR_EXPR);
			break;
		}
	}

	return false;
}

bool ASTBoogieExpressionConverter::visit(FunctionCall const& _node)
{
	// Check for conversions
	if (_node.annotation().kind == FunctionCallKind::TypeConversion)
	{
		auto arg = *_node.arguments().begin();
		// Nothing to do when converting to address
		if (auto expr = dynamic_cast<ElementaryTypeNameExpression const*>(&_node.expression()))
		{
			if (expr->typeName().toString() == ASTBoogieUtils::SOLIDITY_ADDRESS_TYPE)
			{
				arg->accept(*this);
				if (auto lit = dynamic_cast<smack::IntLit const*>(m_currentExpr))
				{
					if (lit->getVal() == 0)
					{
						m_currentExpr = smack::Expr::id(ASTBoogieUtils::BOOGIE_ZERO_ADDRESS);
					}
					else
					{
						reportError(_node.location(), "Unsupported conversion to address");
					}
				}
				return false;
			}
		}
		// Nothing to do when converting to other kind of contract
		if (auto expr = dynamic_cast<Identifier const*>(&_node.expression()))
		{
			if (dynamic_cast<ContractDefinition const *>(expr->annotation().referencedDeclaration))
			{
				arg->accept(*this);
				return false;
			}
		}
		string targetType = ASTBoogieUtils::mapType(_node.annotation().type, _node, m_context.errorReporter(), m_context.bitPrecise());
		string sourceType = ASTBoogieUtils::mapType(arg->annotation().type, *arg, m_context.errorReporter(), m_context.bitPrecise());
		// Nothing to do when the two types are mapped to same type in Boogie,
		// e.g., conversion from uint256 to int256 if both are mapped to int
		if (targetType == sourceType || (targetType == "int" && sourceType == "int_const"))
		{
			arg->accept(*this);
			return false;
		}

		if (m_context.bitPrecise() && sourceType == "int_const")
		{
			arg->accept(*this);
			m_currentExpr = ASTBoogieUtils::checkImplicitBvConversion(m_currentExpr, arg->annotation().type, _node.annotation().type, m_context.bvBuiltinFunctions());
			return false;
		}

		reportError(_node.location(), "Unsupported type conversion");
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
			// TODO: check for implicit bitvector conversion

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
			reportWarning(expMa->location(), "Ignored call to gas() function.");
			expMa->expression().accept(*this);
			return false;
		}
	}

	m_currentExpr = nullptr;
	m_currentAddress = smack::Expr::id(ASTBoogieUtils::BOOGIE_THIS);
	m_currentValue = nullptr;
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
		reportError(_node.location(), "Only identifiers are supported as function calls");
		funcName = ERR_EXPR;
	}

	if (m_isGetter && _node.arguments().size() > 0)
	{
		reportError(_node.location(), "Getter arguments are not supported");
	}

	// Get arguments recursively
	list<const smack::Expr*> args;
	// Pass extra arguments
	if (m_isLibraryCall) { args.push_back(smack::Expr::id(ASTBoogieUtils::BOOGIE_THIS)); } // this
	else { args.push_back(m_currentAddress); } // this
	args.push_back(smack::Expr::id(ASTBoogieUtils::BOOGIE_THIS)); // msg.sender
	smack::Expr const* defaultMsgValue = (m_context.bitPrecise() ? smack::Expr::lit((long)0, 256) : smack::Expr::lit((long)0));
	args.push_back(m_currentValue ? m_currentValue : defaultMsgValue); // msg.value
	if (m_isLibraryCall && !m_isLibraryCallStatic) { args.push_back(m_currentAddress); } // Non-static library calls require extra argument
	// Add normal arguments (except when calling 'call') TODO: is this ok?
	if (funcName != ASTBoogieUtils::BOOGIE_CALL)
	{
		for (unsigned i = 0; i < _node.arguments().size(); ++i)
		{
			auto arg = _node.arguments()[i];
			arg->accept(*this);
			if (m_context.bitPrecise())
			{
				if (auto exprId = dynamic_cast<Identifier const*>(&_node.expression()))
				{
					if (auto funcDef = dynamic_cast<FunctionDefinition const *>(exprId->annotation().referencedDeclaration))
					{
						m_currentExpr = ASTBoogieUtils::checkImplicitBvConversion(m_currentExpr, arg->annotation().type, funcDef->parameters()[i]->annotation().type, m_context.bvBuiltinFunctions());
					}
				}
			}
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
		m_newStatements.push_back(smack::Stmt::assert_(*it, ASTBoogieUtils::createLocAttrs(_node.location(), "Assertion might not hold.", *m_context.currentScanner())));
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

	if (funcName == ASTBoogieUtils::VERIFIER_SUM)
	{
		if (auto id = dynamic_cast<Identifier const*>(&*_node.arguments()[0]))
		{
			auto sumDecl = id->annotation().referencedDeclaration;
			if (find(m_context.currentSumDecls().begin(), m_context.currentSumDecls().end(), sumDecl) == m_context.currentSumDecls().end())
			{
				m_context.currentSumDecls().push_back(sumDecl);
			}

			auto declCategory = id->annotation().type->category();
			if (declCategory != Type::Category::Mapping && declCategory != Type::Category::Array)
			{
				reportError(_node.location(), "Argument of sum must be an array or a mapping");
			}
		}
		else
		{
			reportError(_node.location(), "Argument of sum must be an identifier");
		}
		m_currentExpr = getSumShadowVar(&*_node.arguments()[0]);
		return false;
	}

	// If we set msg.value, we should reduce our own balance
	if (m_currentValue)
	{
		// balance[this] -= msg.value
		m_newStatements.push_back(smack::Stmt::assign(
				smack::Expr::id(ASTBoogieUtils::BOOGIE_BALANCE),
				smack::Expr::upd(
						smack::Expr::id(ASTBoogieUtils::BOOGIE_BALANCE),
						smack::Expr::id(ASTBoogieUtils::BOOGIE_THIS),
						smack::Expr::minus(
								smack::Expr::sel(ASTBoogieUtils::BOOGIE_BALANCE, ASTBoogieUtils::BOOGIE_THIS),
								m_currentValue))));
	}

	if (funcName == ASTBoogieUtils::BOOGIE_CALL)
	{
		for (auto invar : m_context.currentInvars())
		{
			m_newStatements.push_back(smack::Stmt::assert_(invar.first,
					ASTBoogieUtils::createLocAttrs(_node.location(), "Invariant '" + invar.second + "' might not hold before external call.", *m_context.currentScanner())));
		}
	}

	// TODO: check for void return in a more appropriate way
	if (_node.annotation().type->toString() != "tuple()")
	{
		// Create fresh variable to store the result
		smack::Decl* returnVar = smack::Decl::variable(
				funcName + "#" + to_string(_node.id()),
				ASTBoogieUtils::mapType(_node.annotation().type, _node, m_context.errorReporter(), m_context.bitPrecise()));
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
			m_newStatements.push_back(smack::Stmt::annot(ASTBoogieUtils::createLocAttrs(_node.location(), "", *m_context.currentScanner())));
			m_newStatements.push_back(smack::Stmt::call(funcName, args, rets));

			if (funcName == ASTBoogieUtils::BOOGIE_CALL && m_currentValue)
			{
				smack::Block* revert = smack::Block::block();
				// balance[this] -= msg.value
				revert->addStmt(smack::Stmt::assign(
						smack::Expr::id(ASTBoogieUtils::BOOGIE_BALANCE),
						smack::Expr::upd(
								smack::Expr::id(ASTBoogieUtils::BOOGIE_BALANCE),
								smack::Expr::id(ASTBoogieUtils::BOOGIE_THIS),
								smack::Expr::plus(
										smack::Expr::sel(ASTBoogieUtils::BOOGIE_BALANCE, ASTBoogieUtils::BOOGIE_THIS),
										m_currentValue))));
				m_newStatements.push_back(smack::Stmt::ifelse(smack::Expr::not_(m_currentExpr), revert));
			}
		}

	}
	else
	{
		m_currentExpr = nullptr; // No return value for function
		m_newStatements.push_back(smack::Stmt::annot(ASTBoogieUtils::createLocAttrs(_node.location(), "", *m_context.currentScanner())));
		m_newStatements.push_back(smack::Stmt::call(funcName, args));
	}

	if (funcName == ASTBoogieUtils::BOOGIE_CALL)
	{
		for (auto invar : m_context.currentInvars()) m_newStatements.push_back(smack::Stmt::assume(invar.first));
	}

	return false;
}

bool ASTBoogieExpressionConverter::visit(NewExpression const& _node)
{
	reportError(_node.location(), "New expression is not supported");
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
		reportError(_node.location(), "Member without corresponding declaration (probably an unsupported special member)");
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
		reportError(_node.location(), "Identifier '" + _node.name() + "' has no matching declaration");
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
	string tpStr = _node.annotation().type->toString();
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

	reportError(_node.location(), "Unsupported literal for type " + tpStr.substr(0, tpStr.find(' ')));
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
