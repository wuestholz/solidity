#include <boost/algorithm/string/predicate.hpp>
#include <libsolidity/ast/ASTBoogieExpressionConverter.h>
#include <libsolidity/ast/ASTBoogieUtils.h>
#include <libsolidity/ast/AST.h>
#include <liblangutil/Exceptions.h>

using namespace std;
using namespace dev;
using namespace dev::solidity;
using namespace langutil;

namespace bg = boogie;

namespace dev
{
namespace solidity
{

bg::Expr::Ref ASTBoogieExpressionConverter::getArrayLength(bg::Expr::Ref expr, ASTNode const& associatedNode)
{
	// Array is a local variable
	if (auto localArray = dynamic_pointer_cast<bg::VarExpr const>(expr))
		return bg::Expr::id(localArray->name() + ASTBoogieUtils::BOOGIE_LENGTH);
	// Array is state variable
	if (auto stateArray = dynamic_pointer_cast<bg::SelExpr const>(expr))
		if (auto baseArray = dynamic_pointer_cast<bg::VarExpr const>(stateArray->getBase()))
		{
			return bg::Expr::sel(
							bg::Expr::id(baseArray->name() + ASTBoogieUtils::BOOGIE_LENGTH),
							stateArray->getIdxs());
		}

	m_context.reportError(&associatedNode, "Unsupported access to array length");
	return bg::Expr::id(ASTBoogieUtils::ERR_EXPR);
}

bg::Expr::Ref ASTBoogieExpressionConverter::getSumShadowVar(ASTNode const* node)
{
	if (auto sumBase = dynamic_cast<Identifier const*>(node))
		if (auto sumBaseDecl = sumBase->annotation().referencedDeclaration)
		{
			return bg::Expr::sel(
					bg::Expr::id(ASTBoogieUtils::mapDeclName(*sumBaseDecl) + ASTBoogieUtils::BOOGIE_SUM),
					bg::Expr::id(ASTBoogieUtils::BOOGIE_THIS));
		}

	m_context.reportError(node, "Unsupported identifier for sum function");
	return bg::Expr::id(ASTBoogieUtils::ERR_EXPR);
}

void ASTBoogieExpressionConverter::addTCC(bg::Expr::Ref expr, TypePointer tp)
{
	if (m_context.encoding() == BoogieContext::Encoding::MOD && ASTBoogieUtils::isBitPreciseType(tp))
		m_tccs.push_back(ASTBoogieUtils::getTCCforExpr(expr, tp));
}

void ASTBoogieExpressionConverter::addSideEffect(bg::Stmt::Ref stmt)
{
	for (auto oc : m_ocs)
	{
		m_newStatements.push_back(bg::Stmt::assign(
			bg::Expr::id(ASTBoogieUtils::VERIFIER_OVERFLOW),
			bg::Expr::or_(bg::Expr::id(ASTBoogieUtils::VERIFIER_OVERFLOW), bg::Expr::not_(oc))));
	}
	m_ocs.clear();
	m_newStatements.push_back(stmt);
}

ASTBoogieExpressionConverter::ASTBoogieExpressionConverter(BoogieContext& context) :
		m_context(context),
		m_currentExpr(nullptr),
		m_currentAddress(nullptr),
		m_currentMsgValue(nullptr),
		m_isGetter(false),
		m_isLibraryCall(false),
		m_isLibraryCallStatic(false) {}

ASTBoogieExpressionConverter::Result ASTBoogieExpressionConverter::convert(const Expression& _node)
{
	m_currentExpr = nullptr;
	m_currentAddress = nullptr;
	m_currentMsgValue = nullptr;
	m_isGetter = false;
	m_isLibraryCall = false;
	m_isLibraryCallStatic = false;
	m_newStatements.clear();
	m_newDecls.clear();
	m_newConstants.clear();
	m_tccs.clear();
	m_ocs.clear();

	_node.accept(*this);

	return Result(m_currentExpr, m_newStatements, m_newDecls, m_newConstants, m_tccs, m_ocs);
}

bool ASTBoogieExpressionConverter::visit(Conditional const& _node)
{
	// Get condition recursively
	_node.condition().accept(*this);
	bg::Expr::Ref cond = m_currentExpr;

	// Get true expression recursively
	_node.trueExpression().accept(*this);
	bg::Expr::Ref trueExpr = m_currentExpr;

	// Get false expression recursively
	_node.falseExpression().accept(*this);
	bg::Expr::Ref falseExpr = m_currentExpr;

	m_currentExpr = bg::Expr::cond(cond, trueExpr, falseExpr);
	return false;
}

bool ASTBoogieExpressionConverter::visit(Assignment const& _node)
{
	// Get lhs recursively
	_node.leftHandSide().accept(*this);
	bg::Expr::Ref lhs = m_currentExpr;

	// Get rhs recursively
	_node.rightHandSide().accept(*this);
	bg::Expr::Ref rhs = m_currentExpr;

	// Result will be the LHS (for chained assignments like x = y = 5)
	m_currentExpr = lhs;

	unsigned bits = ASTBoogieUtils::getBits(_node.leftHandSide().annotation().type);
	bool isSigned = ASTBoogieUtils::isSigned(_node.leftHandSide().annotation().type);

	// Bit-precise mode
	if (m_context.isBvEncoding() && ASTBoogieUtils::isBitPreciseType(_node.leftHandSide().annotation().type))
		// Check for implicit conversion
		rhs = ASTBoogieUtils::checkImplicitBvConversion(rhs, _node.rightHandSide().annotation().type, _node.leftHandSide().annotation().type, m_context);

	// Transform rhs based on the operator, e.g., a += b becomes a := bvadd(a, b)
	ASTBoogieUtils::expr_pair result;
	switch(_node.assignmentOperator())
	{
	case Token::Assign: result.first = rhs; break; // rhs already contains the result
	case Token::AssignAdd: result = ASTBoogieUtils::encodeArithBinaryOp(m_context, &_node, Token::Add, lhs, rhs, bits, isSigned); break;
	case Token::AssignSub: result = ASTBoogieUtils::encodeArithBinaryOp(m_context, &_node, Token::Sub, lhs, rhs, bits, isSigned); break;
	case Token::AssignMul: result = ASTBoogieUtils::encodeArithBinaryOp(m_context, &_node, Token::Mul, lhs, rhs, bits, isSigned); break;
	case Token::AssignDiv: result = ASTBoogieUtils::encodeArithBinaryOp(m_context, &_node, Token::Div, lhs, rhs, bits, isSigned); break;
	case Token::AssignMod: result = ASTBoogieUtils::encodeArithBinaryOp(m_context, &_node, Token::Mod, lhs, rhs, bits, isSigned); break;
	case Token::AssignBitAnd: result = ASTBoogieUtils::encodeArithBinaryOp(m_context, &_node, Token::BitAnd, lhs, rhs, bits, isSigned); break;
	case Token::AssignBitOr: result = ASTBoogieUtils::encodeArithBinaryOp(m_context, &_node, Token::BitOr, lhs, rhs, bits, isSigned); break;
	case Token::AssignBitXor: result = ASTBoogieUtils::encodeArithBinaryOp(m_context, &_node, Token::BitXor, lhs, rhs, bits, isSigned); break;
	default:
		m_context.reportError(&_node, string("Unsupported assignment operator: ") + TokenTraits::toString(_node.assignmentOperator()));
		m_currentExpr = bg::Expr::id(ASTBoogieUtils::ERR_EXPR);
		return false;
	}

	if (m_context.overflow() && result.second)
		m_ocs.push_back(result.second);

	// Create the assignment with the helper method
	createAssignment(_node.leftHandSide(), lhs, result.first);
	return false;
}

void ASTBoogieExpressionConverter::createAssignment(Expression const& originalLhs, bg::Expr::Ref lhs, bg::Expr::Ref rhs)
{
	// First check if shadow variables need to be updated
	if (auto lhsIdx = dynamic_cast<IndexAccess const*>(&originalLhs))
	{
		if (auto lhsId = dynamic_cast<Identifier const*>(&lhsIdx->baseExpression()))
		{
			if (m_context.currentSumDecls()[lhsId->annotation().referencedDeclaration])
			{
				// arr[i] = x becomes arr#sum := arr#sum[this := ((arr#sum[this] - arr[i]) + x)]
				auto sumId = bg::Expr::id(ASTBoogieUtils::mapDeclName(*lhsId->annotation().referencedDeclaration) + ASTBoogieUtils::BOOGIE_SUM);

				unsigned bits = ASTBoogieUtils::getBits(originalLhs.annotation().type);
				bool isSigned = ASTBoogieUtils::isSigned(originalLhs.annotation().type);

				auto selExpr = bg::Expr::sel(sumId, bg::Expr::id(ASTBoogieUtils::BOOGIE_THIS));
				auto subResult = ASTBoogieUtils::encodeArithBinaryOp(m_context, nullptr, Token::Sub, selExpr, lhs, bits, isSigned);
				auto updResult = ASTBoogieUtils::encodeArithBinaryOp(m_context, nullptr, Token::Add, subResult.first, rhs, bits, isSigned);
				if (m_context.overflow())
				{
					addSideEffect(bg::Stmt::comment("Implicit assumption that unsigned sum cannot underflow."));
					addSideEffect(bg::Stmt::assume(subResult.second));
				}
				addSideEffect(bg::Stmt::assign(
						sumId,
						bg::Expr::upd(sumId, bg::Expr::id(ASTBoogieUtils::BOOGIE_THIS), updResult.first)));

			}
		}
	}


	// If LHS is simply an identifier, we can assign to it
	if (dynamic_pointer_cast<bg::VarExpr const>(lhs))
	{
		addSideEffect(bg::Stmt::assign(lhs, rhs));
		return;
	}

	// If LHS is an indexer (arrays/maps), it needs to be transformed to an update
	if (auto lhsSel = dynamic_pointer_cast<bg::SelExpr const>(lhs))
	{
		if (auto lhsUpd = dynamic_pointer_cast<bg::UpdExpr const>(selectToUpdate(lhsSel, rhs)))
		{
			addSideEffect(bg::Stmt::assign(lhsUpd->getBase(), lhsUpd));
			return;
		}
	}

	m_context.reportError(&originalLhs, "Unsupported assignment (LHS must be identifier/indexer)");
}

bg::Expr::Ref ASTBoogieExpressionConverter::selectToUpdate(std::shared_ptr<bg::SelExpr const> sel, bg::Expr::Ref value)
{
	if (auto base = dynamic_pointer_cast<bg::SelExpr const>(sel->getBase()))
		return selectToUpdate(base, bg::Expr::upd(base, sel->getIdxs(), value));
	else
		return bg::Expr::upd(sel->getBase(), sel->getIdxs(), value);
}

bool ASTBoogieExpressionConverter::visit(TupleExpression const& _node)
{
	if (_node.components().size() == 1)
		_node.components()[0]->accept(*this);
	else
	{
		m_context.reportError(&_node, "Tuples are not supported");
		m_currentExpr = bg::Expr::id(ASTBoogieUtils::ERR_EXPR);
	}
	return false;
}

bool ASTBoogieExpressionConverter::visit(UnaryOperation const& _node)
{
	// Check if constant propagation could infer the result
	string tpStr = _node.annotation().type->toString();
	if (boost::starts_with(tpStr, "int_const"))
	{
		m_currentExpr = bg::Expr::lit(bg::bigint(tpStr.substr(10)));
		return false;
	}

	// Get operand recursively
	_node.subExpression().accept(*this);
	bg::Expr::Ref subExpr = m_currentExpr;

	switch (_node.getOperator()) {
	case Token::Add: m_currentExpr = subExpr; break; // Unary plus does not do anything
	case Token::Not: m_currentExpr = bg::Expr::not_(subExpr); break;

	case Token::Sub:
	case Token::BitNot: {
		unsigned bits = ASTBoogieUtils::getBits(_node.annotation().type);
		bool isSigned = ASTBoogieUtils::isSigned(_node.annotation().type);
		auto exprResult = ASTBoogieUtils::encodeArithUnaryOp(m_context, &_node, _node.getOperator(), subExpr, bits, isSigned);
		m_currentExpr = exprResult.first;
		if (m_context.overflow() && exprResult.second)
			m_ocs.push_back(exprResult.second);
		break;
	}

	// Inc and Dec shares most part of the code
	case Token::Inc:
	case Token::Dec:
		{
			unsigned bits = ASTBoogieUtils::getBits(_node.annotation().type);
			bool isSigned = ASTBoogieUtils::isSigned(_node.annotation().type);
			bg::Expr::Ref lhs = subExpr;
			auto rhsResult =
					ASTBoogieUtils::encodeArithBinaryOp(m_context, &_node,
							_node.getOperator() == Token::Inc ? Token::Add : Token::Sub,
							lhs,
							m_context.isBvEncoding() ? bg::Expr::lit(bg::bigint(1), bits) : bg::Expr::lit(bg::bigint(1)),
							bits, isSigned);
			bg::Decl::Ref tempVar = bg::Decl::variable("inc#" + to_string(_node.id()),
					ASTBoogieUtils::mapType(_node.subExpression().annotation().type, _node, m_context));
			m_newDecls.push_back(tempVar);
			if (_node.isPrefixOperation()) // ++x (or --x)
			{
				// First do the assignment x := x + 1 (or x := x - 1)
				if (m_context.overflow() && rhsResult.second) { m_ocs.push_back(rhsResult.second); }
				createAssignment(_node.subExpression(), lhs, rhsResult.first);
				// Then the assignment tmp := x
				addSideEffect(bg::Stmt::assign(bg::Expr::id(tempVar->getName()), lhs));
			}
			else // x++ (or x--)
			{
				// First do the assignment tmp := x
				addSideEffect(bg::Stmt::assign(bg::Expr::id(tempVar->getName()), subExpr));
				// Then the assignment x := x + 1 (or x := x - 1)
				if (m_context.overflow() && rhsResult.second) { m_ocs.push_back(rhsResult.second); }
				createAssignment(_node.subExpression(), lhs, rhsResult.first);
			}
			// Result is the tmp variable (if the assignment is part of an expression)
			m_currentExpr = bg::Expr::id(tempVar->getName());
		}
		break;
	default:
		m_context.reportError(&_node, string("Unsupported unary operator: ") + TokenTraits::toString(_node.getOperator()));
		m_currentExpr = bg::Expr::id(ASTBoogieUtils::ERR_EXPR);
		break;
	}

	return false;
}

bool ASTBoogieExpressionConverter::visit(BinaryOperation const& _node)
{
	// Check if constant propagation could infer the result
	string tpStr = _node.annotation().type->toString();
	if (boost::starts_with(tpStr, "int_const"))
	{
		m_currentExpr = bg::Expr::lit(bg::bigint(tpStr.substr(10)));
		return false;
	}

	// Get lhs recursively
	_node.leftExpression().accept(*this);
	bg::Expr::Ref lhs = m_currentExpr;

	// Get rhs recursively
	_node.rightExpression().accept(*this);
	bg::Expr::Ref rhs = m_currentExpr;

	// Common type might not be equal to the type of the node, e.g., in case of uint32 == uint64,
	// the common type is uint64, but the type of the node is bool
	auto commonType = _node.leftExpression().annotation().type->binaryOperatorResult(_node.getOperator(), _node.rightExpression().annotation().type);

	// Check implicit conversion for bitvectors
	if (m_context.isBvEncoding() && ASTBoogieUtils::isBitPreciseType(commonType))
	{
		lhs = ASTBoogieUtils::checkImplicitBvConversion(lhs, _node.leftExpression().annotation().type, commonType, m_context);
		rhs = ASTBoogieUtils::checkImplicitBvConversion(rhs, _node.rightExpression().annotation().type, commonType, m_context);
	}

	auto op = _node.getOperator();
	switch(op)
	{
	// Non-arithmetic operations
	case Token::And: m_currentExpr = bg::Expr::and_(lhs, rhs); break;
	case Token::Or: m_currentExpr = bg::Expr::or_(lhs, rhs); break;
	case Token::Equal: m_currentExpr = bg::Expr::eq(lhs, rhs); break;
	case Token::NotEqual: m_currentExpr = bg::Expr::neq(lhs, rhs); break;

	// Arithmetic operations
	case Token::Add:
	case Token::Sub:
	case Token::Mul:
	case Token::Div:
	case Token::Mod:
	case Token::Exp:

	case Token::LessThan:
	case Token::GreaterThan:
	case Token::LessThanOrEqual:
	case Token::GreaterThanOrEqual:

	case Token::BitAnd:
	case Token::BitOr:
	case Token::BitXor:
	case Token::SHL:
	case Token::SAR: {
		unsigned bits = ASTBoogieUtils::getBits(commonType);
		bool isSigned = ASTBoogieUtils::isSigned(commonType);
		auto exprResult = ASTBoogieUtils::encodeArithBinaryOp(m_context, &_node, _node.getOperator(), lhs, rhs, bits, isSigned);
		m_currentExpr = exprResult.first;
		if (m_context.overflow() && exprResult.second)
			m_ocs.push_back(exprResult.second);
		break;
	}

	default:
		m_context.reportError(&_node, string("Unsupported binary operator ") + TokenTraits::toString(_node.getOperator()));
		m_currentExpr = bg::Expr::id(ASTBoogieUtils::ERR_EXPR);
		break;
	}

	return false;
}

bool ASTBoogieExpressionConverter::visit(FunctionCall const& _node)
{
	// Check for conversions
	if (_node.annotation().kind == FunctionCallKind::TypeConversion)
	{
		if (_node.arguments().size() != 1)
		{
			BOOST_THROW_EXCEPTION(InternalCompilerError() <<
							errinfo_comment("Type conversion should have exactly one argument") <<
							errinfo_sourceLocation(_node.location()));
		}
		auto arg = *_node.arguments().begin();
		// Converting to address
		if (auto expr = dynamic_cast<ElementaryTypeNameExpression const*>(&_node.expression()))
		{
			if (expr->typeName().token() == Token::Address)
			{
				arg->accept(*this);
				if (auto lit = dynamic_pointer_cast<bg::IntLit const>(m_currentExpr))
				{
					if (lit->getVal() == 0)
						m_currentExpr = bg::Expr::id(ASTBoogieUtils::BOOGIE_ZERO_ADDRESS);
					else
						m_context.reportError(&_node, "Unsupported conversion to address");
				}
				return false;
			}
		}
		// Converting to other kind of contract
		if (auto expr = dynamic_cast<Identifier const*>(&_node.expression()))
		{
			if (dynamic_cast<ContractDefinition const *>(expr->annotation().referencedDeclaration))
			{
				arg->accept(*this);
				return false;
			}
		}
		string targetType = ASTBoogieUtils::mapType(_node.annotation().type, _node, m_context);
		string sourceType = ASTBoogieUtils::mapType(arg->annotation().type, *arg, m_context);
		// Nothing to do when the two types are mapped to same type in Boogie,
		// e.g., conversion from uint256 to int256 if both are mapped to int
		if (targetType == sourceType || (targetType == "int" && sourceType == "int_const"))
		{
			arg->accept(*this);
			return false;
		}

		if (m_context.isBvEncoding() && ASTBoogieUtils::isBitPreciseType(_node.annotation().type))
		{
			arg->accept(*this);
			m_currentExpr = ASTBoogieUtils::checkExplicitBvConversion(m_currentExpr, arg->annotation().type, _node.annotation().type, m_context);
			return false;
		}

		m_context.reportError(&_node, "Unsupported type conversion");
		m_currentExpr = bg::Expr::id(ASTBoogieUtils::ERR_EXPR);
		return false;
	}
	// Function calls in Boogie are statements and cannot be part of
	// expressions, therefore each function call is given a fresh variable
	// for its return value and is mapped to a call statement

	// First, process the expression of the function call, which should give:
	// - The name of the called function in 'm_currentExpr'
	// - The address on which the function is called in 'm_currentAddress'
	// - The msg.value in 'm_currentMsgValue'
	// Example: f(z) gives 'f' as the name and 'this' as the address
	// Example: x.f(z) gives 'f' as the name and 'x' as the address
	// Example: x.f.value(y)(z) gives 'f' as the name, 'x' as the address and 'y' as the value

	// Check for the special case of calling the 'value' function
	// For example x.f.value(y)(z) should be treated as x.f(z), while setting
	// 'm_currentMsgValue' to 'y'.
	if (auto exprMa = dynamic_cast<const MemberAccess*>(&_node.expression()))
	{
		if (exprMa->memberName() == "value")
		{
			// Process the argument
			if (_node.arguments().size() != 1)
			{
				BOOST_THROW_EXCEPTION(InternalCompilerError() <<
								errinfo_comment("Call to the value function should have exactly one argument") <<
								errinfo_sourceLocation(_node.location()));
			}
			auto arg = *_node.arguments().begin();
			arg->accept(*this);
			m_currentMsgValue = m_currentExpr;
			if (m_context.isBvEncoding())
			{
				m_currentMsgValue = ASTBoogieUtils::checkImplicitBvConversion(m_currentMsgValue,
						arg->annotation().type, make_shared<IntegerType>(256, IntegerType::Modifier::Unsigned), m_context);
			}

			// Continue with the rest of the AST
			exprMa->expression().accept(*this);
			return false;
		}
	}

	// Ignore gas setting, e.g., x.f.gas(y)(z) is just x.f(z)
	if (auto exprMa = dynamic_cast<const MemberAccess*>(&_node.expression()))
	{
		if (exprMa->memberName() == "gas")
		{
			m_context.reportWarning(exprMa, "Ignored call to gas() function.");
			exprMa->expression().accept(*this);
			return false;
		}
	}

	m_currentExpr = nullptr;
	m_currentAddress = bg::Expr::id(ASTBoogieUtils::BOOGIE_THIS);
	m_currentMsgValue = nullptr;
	m_isGetter = false;
	m_isLibraryCall = false;
	// Process expression
	_node.expression().accept(*this);

	// 'm_currentExpr' should be an identifier, giving the name of the function
	string funcName;
	if (auto varExpr = dynamic_pointer_cast<bg::VarExpr const>(m_currentExpr))
		funcName = varExpr->name();
	else
	{
		m_context.reportError(&_node, "Only identifiers are supported as function calls");
		funcName = ASTBoogieUtils::ERR_EXPR;
	}

	if (m_isGetter && _node.arguments().size() > 0)
		m_context.reportError(&_node, "Getter arguments are not supported");

	// Process arguments recursively
	list<bg::Expr::Ref> args;
	// First, pass extra arguments
	if (m_isLibraryCall) { args.push_back(bg::Expr::id(ASTBoogieUtils::BOOGIE_THIS)); } // this
	else { args.push_back(m_currentAddress); } // this
	args.push_back(bg::Expr::id(ASTBoogieUtils::BOOGIE_THIS)); // msg.sender
	bg::Expr::Ref defaultMsgValue = (m_context.isBvEncoding() ?
			bg::Expr::lit(bg::bigint(0), 256) : bg::Expr::lit(bg::bigint(0)));
	bg::Expr::Ref msgValue = m_currentMsgValue ? m_currentMsgValue : defaultMsgValue;
	args.push_back(msgValue); // msg.value
	if (m_isLibraryCall && !m_isLibraryCallStatic) { args.push_back(m_currentAddress); } // Non-static library calls require extra argument

	for (unsigned i = 0; i < _node.arguments().size(); ++i)
	{
		auto arg = _node.arguments()[i];
		arg->accept(*this);
		// In bit-precise mode, we need the definition of the function to see the types of the parameters
		// (for implicit conversions)
		if (m_context.isBvEncoding())
		{
			// Try to get the definition
			const FunctionDefinition* calledFunc = nullptr;
			if (auto exprId = dynamic_cast<Identifier const*>(&_node.expression()))
			{
				if (auto funcDef = dynamic_cast<FunctionDefinition const *>(exprId->annotation().referencedDeclaration)) { calledFunc = funcDef; }
			}
			if (auto exprMa = dynamic_cast<MemberAccess const*>(&_node.expression()))
			{
				if (auto funcDef = dynamic_cast<FunctionDefinition const *>(exprMa->annotation().referencedDeclaration)) { calledFunc = funcDef; }
			}
			// TODO: the above does not work for complex expressions like 'x.f.value(1)()'

			if (calledFunc)
				m_currentExpr = ASTBoogieUtils::checkImplicitBvConversion(m_currentExpr, arg->annotation().type, calledFunc->parameters()[i]->annotation().type, m_context);
		}

		// Do not add argument for call
		if (funcName != ASTBoogieUtils::BOOGIE_CALL)
		{
			args.push_back(m_currentExpr);
			// Array lenght
			if (arg->annotation().type && arg->annotation().type->category() == Type::Category::Array)
				args.push_back(getArrayLength(m_currentExpr, *arg));
		}
	}

	// Check for calls to special functions

	// Assert is a separate statement in Boogie (instead of a function call)
	if (funcName == ASTBoogieUtils::SOLIDITY_ASSERT)
	{
		if (_node.arguments().size() != 1)
			BOOST_THROW_EXCEPTION(InternalCompilerError() <<
							errinfo_comment("Assert should have exactly one argument") <<
							errinfo_sourceLocation(_node.location()));

		// Parameter of assert is the first (and only) normal argument
		list<bg::Expr::Ref>::iterator it = args.begin();
		std::advance(it, args.size() - _node.arguments().size());
		addSideEffect(bg::Stmt::assert_(*it, ASTBoogieUtils::createAttrs(_node.location(), "Assertion might not hold.", *m_context.currentScanner())));
		return false;
	}

	// Require is mapped to assume statement in Boogie (instead of a function call)
	if (funcName == ASTBoogieUtils::SOLIDITY_REQUIRE)
	{
		if (2 < _node.arguments().size() || _node.arguments().size() < 1)
			BOOST_THROW_EXCEPTION(InternalCompilerError() <<
							errinfo_comment("Require should have one or two argument(s)") <<
							errinfo_sourceLocation(_node.location()));

		// Parameter of assume is the first normal argument (second is optional message omitted in Boogie)
		list<bg::Expr::Ref>::iterator it = args.begin();
		std::advance(it, args.size() - _node.arguments().size());
		addSideEffect(bg::Stmt::assume(*it));
		return false;
	}

	// Revert is mapped to assume(false) statement in Boogie (instead of a function call)
	// Its argument is an optional message, omitted in Boogie
	if (funcName == ASTBoogieUtils::SOLIDITY_REVERT)
	{
		if (_node.arguments().size() > 1)
			BOOST_THROW_EXCEPTION(InternalCompilerError() <<
							errinfo_comment("Revert should have at most one argument") <<
							errinfo_sourceLocation(_node.location()));

		addSideEffect(bg::Stmt::assume(bg::Expr::lit(false)));
		return false;
	}

	// Sum function
	if (boost::algorithm::starts_with(funcName, ASTBoogieUtils::VERIFIER_SUM))
	{
		TypePointer tp = nullptr;
		if (auto id = dynamic_cast<Identifier const*>(&*_node.arguments()[0]))
		{
			auto sumDecl = id->annotation().referencedDeclaration;

			MagicVariableDeclaration const* magicVar = nullptr;
			if (auto exprId = dynamic_cast<Identifier const*>(&_node.expression()))
				magicVar = dynamic_cast<MagicVariableDeclaration const *>(exprId->annotation().referencedDeclaration);

			if (magicVar)
			{
				m_context.currentSumDecls()[sumDecl] = dynamic_cast<FunctionType const*>(&*magicVar->type())->returnParameterTypes()[0];
				tp = m_context.currentSumDecls()[sumDecl];
			}
			else { m_context.reportError(&_node, "Could not find sum function"); }

			auto declCategory = id->annotation().type->category();
			if (declCategory != Type::Category::Mapping && declCategory != Type::Category::Array)
				m_context.reportError(&_node, "Argument of sum must be an array or a mapping");
		}
		else
			m_context.reportError(&_node, "Argument of sum must be an identifier");

		m_currentExpr = getSumShadowVar(&*_node.arguments()[0]);
		addTCC(m_currentExpr, tp);
		return false;
	}

	// If msg.value was set, we should reduce our own balance (and the called function will increase its own)
	if (msgValue != defaultMsgValue)
	{
		// assert(balance[this] >= msg.value)
		auto selExpr = bg::Expr::sel(ASTBoogieUtils::BOOGIE_BALANCE, ASTBoogieUtils::BOOGIE_THIS);
		auto geqResult = ASTBoogieUtils::encodeArithBinaryOp(m_context, nullptr, langutil::Token::GreaterThanOrEqual, selExpr, msgValue, 256, false);
		addSideEffect(bg::Stmt::comment("Implicit assumption that we have enough ether"));
		addSideEffect(bg::Stmt::assume(geqResult.first));
		// balance[this] -= msg.value
		bg::Expr::Ref this_balance = bg::Expr::sel(ASTBoogieUtils::BOOGIE_BALANCE, ASTBoogieUtils::BOOGIE_THIS);
		if (m_context.encoding() == BoogieContext::Encoding::MOD)
		{
			TypePointer tp_uint256 = make_shared<IntegerType>(256, IntegerType::Modifier::Unsigned);
			addSideEffect(bg::Stmt::assume(ASTBoogieUtils::getTCCforExpr(this_balance, tp_uint256)));
			addSideEffect(bg::Stmt::assume(ASTBoogieUtils::getTCCforExpr(msgValue, tp_uint256)));
		}
		auto subResult = ASTBoogieUtils::encodeArithBinaryOp(m_context, nullptr, Token::Sub,
												this_balance, msgValue, 256, false);
		if (m_context.overflow())
		{
			addSideEffect(bg::Stmt::comment("Implicit assumption that balances cannot overflow"));
			addSideEffect(bg::Stmt::assume(subResult.second));
		}
		addSideEffect(bg::Stmt::assign(
				bg::Expr::id(ASTBoogieUtils::BOOGIE_BALANCE),
				bg::Expr::upd(
						bg::Expr::id(ASTBoogieUtils::BOOGIE_BALANCE),
						bg::Expr::id(ASTBoogieUtils::BOOGIE_THIS),
						subResult.first)));
	}

	// External calls require the invariants to hold
	if (funcName == ASTBoogieUtils::BOOGIE_CALL)
	{
		for (auto invar : m_context.currentContractInvars())
		{
			for (auto tcc : invar.tccs) { addSideEffect(bg::Stmt::assert_(tcc,
										ASTBoogieUtils::createAttrs(_node.location(), "Variables for invariant '" + invar.exprStr + "' might be out of range before external call.", *m_context.currentScanner()))); }
			addSideEffect(bg::Stmt::assert_(invar.expr,
					ASTBoogieUtils::createAttrs(_node.location(), "Invariant '" + invar.exprStr + "' might not hold before external call.", *m_context.currentScanner())));
		}
	}

	// TODO: check for void return in a more appropriate way
	if (_node.annotation().type->toString() != "tuple()")
	{
		// Create fresh variable to store the result
		bg::Decl::Ref returnVar = bg::Decl::variable(
				funcName + "#" + to_string(_node.id()),
				ASTBoogieUtils::mapType(_node.annotation().type, _node, m_context));
		m_newDecls.push_back(returnVar);
		// Result of the function call is the fresh variable
		m_currentExpr = bg::Expr::id(returnVar->getName());

		if (m_isGetter)
		{
			// Getters are replaced with map access (instead of function call)
			addSideEffect(bg::Stmt::assign(
					bg::Expr::id(returnVar->getName()),
					bg::Expr::sel(bg::Expr::id(funcName), m_currentAddress)));
		}
		else
		{
			// Assign call to the fresh variable
			addSideEffect(bg::Stmt::annot(ASTBoogieUtils::createAttrs(_node.location(), "", *m_context.currentScanner())));
			addSideEffect(bg::Stmt::call(funcName, args, {returnVar->getName()}));

			// The call function is special as it indicates failure in a return value and in this case
			// we must undo reducing our balance
			if (funcName == ASTBoogieUtils::BOOGIE_CALL && msgValue != defaultMsgValue)
			{
				bg::Block::Ref revert = bg::Block::block();
				// balance[this] += msg.value
				bg::Expr::Ref this_balance = bg::Expr::sel(ASTBoogieUtils::BOOGIE_BALANCE, ASTBoogieUtils::BOOGIE_THIS);
				if (m_context.encoding() == BoogieContext::Encoding::MOD)
				{
					TypePointer tp_uint256 = make_shared<IntegerType>(256, IntegerType::Modifier::Unsigned);
					revert->addStmt(bg::Stmt::assume(ASTBoogieUtils::getTCCforExpr(this_balance, tp_uint256)));
					revert->addStmt(bg::Stmt::assume(ASTBoogieUtils::getTCCforExpr(msgValue, tp_uint256)));
				}
				auto addResult = ASTBoogieUtils::encodeArithBinaryOp(m_context, nullptr, Token::Add,
															this_balance, msgValue, 256, false);
				if (m_context.overflow())
				{
					revert->addStmt(bg::Stmt::comment("Implicit assumption that balances cannot overflow"));
					revert->addStmt(bg::Stmt::assume(addResult.second));
				}
				revert->addStmt(bg::Stmt::assign(
						bg::Expr::id(ASTBoogieUtils::BOOGIE_BALANCE),
						bg::Expr::upd(
								bg::Expr::id(ASTBoogieUtils::BOOGIE_BALANCE),
								bg::Expr::id(ASTBoogieUtils::BOOGIE_THIS),
								addResult.first)));
				addSideEffect(bg::Stmt::ifelse(bg::Expr::not_(m_currentExpr), revert));
			}
		}

	}
	else // No return value for function
	{
		m_currentExpr = nullptr;
		addSideEffect(bg::Stmt::annot(ASTBoogieUtils::createAttrs(_node.location(), "", *m_context.currentScanner())));
		addSideEffect(bg::Stmt::call(funcName, args));
	}

	// Assume invariants after external call
	if (funcName == ASTBoogieUtils::BOOGIE_CALL)
	{

		for (auto invar : m_context.currentContractInvars()) {
			for (auto tcc : invar.tccs) { addSideEffect(bg::Stmt::assume(tcc)); }
			addSideEffect(bg::Stmt::assume(invar.expr));
		}
	}

	return false;
}

bool ASTBoogieExpressionConverter::visit(NewExpression const& _node)
{
	m_context.reportError(&_node, "New expression is not supported");
	m_currentExpr = bg::Expr::id(ASTBoogieUtils::ERR_EXPR);
	return false;
}

bool ASTBoogieExpressionConverter::visit(MemberAccess const& _node)
{
	// Normally, the expression of the MemberAccess will give the address and
	// the memberName will give the name. For example, x.f() will have address
	// 'x' and name 'f'.

	// Get expression recursively
	_node.expression().accept(*this);
	bg::Expr::Ref expr = m_currentExpr;
	// The current expression gives the address on which something is done
	m_currentAddress = m_currentExpr;
	// If we are accessing something on 'super', the current address should be 'this'
	// and not 'super', because that does not exist
	if (auto id = dynamic_cast<Identifier const*>(&_node.expression()))
		if (id->annotation().referencedDeclaration->name() == ASTBoogieUtils::SOLIDITY_SUPER)
			m_currentAddress = bg::Expr::id(ASTBoogieUtils::BOOGIE_THIS);

	// Type of the expression
	TypePointer type = _node.expression().annotation().type;
	Type::Category typeCategory = type->category();

	// Handle special members/functions

	// address.balance / this.balance
	bool isAddress = typeCategory == Type::Category::Address;
	if (_node.memberName() == ASTBoogieUtils::SOLIDITY_BALANCE)
	{
		if (isAddress) {
			m_currentExpr = bg::Expr::sel(bg::Expr::id(ASTBoogieUtils::BOOGIE_BALANCE), expr);
			addTCC(m_currentExpr, make_shared<IntegerType>(256, IntegerType::Modifier::Unsigned));
			return false;
		}
		if (auto exprId = dynamic_cast<Identifier const*>(&_node.expression()))
		{
			if (exprId->name() == ASTBoogieUtils::SOLIDITY_THIS)
			{
				m_currentExpr = bg::Expr::sel(ASTBoogieUtils::BOOGIE_BALANCE, ASTBoogieUtils::BOOGIE_THIS);
				addTCC(m_currentExpr, make_shared<IntegerType>(256, IntegerType::Modifier::Unsigned));
				return false;
			}
		}
	}
	// address.transfer()
	if (isAddress && _node.memberName() == ASTBoogieUtils::SOLIDITY_TRANSFER)
	{
		m_context.includeTransferFunction();
		m_currentExpr = bg::Expr::id(ASTBoogieUtils::BOOGIE_TRANSFER);
		return false;
	}
	// address.send()
	if (isAddress && _node.memberName() == ASTBoogieUtils::SOLIDITY_SEND)
	{
		m_context.includeSendFunction();
		m_currentExpr = bg::Expr::id(ASTBoogieUtils::BOOGIE_SEND);
		return false;
	}
	// address.call()
	if (isAddress && _node.memberName() == ASTBoogieUtils::SOLIDITY_CALL)
	{
		m_context.includeCallFunction();
		m_currentExpr = bg::Expr::id(ASTBoogieUtils::BOOGIE_CALL);
		return false;
	}
	// msg.sender
	auto magicType = std::dynamic_pointer_cast<MagicType const>(type);
	bool isMessage = magicType != nullptr && magicType->kind() == MagicType::Kind::Message;
	if (isMessage && _node.memberName() == ASTBoogieUtils::SOLIDITY_SENDER)
	{
		m_currentExpr = bg::Expr::id(ASTBoogieUtils::BOOGIE_MSG_SENDER);
		return false;
	}
	// msg.value
	if (isMessage && _node.memberName() == ASTBoogieUtils::SOLIDITY_VALUE)
	{
		m_currentExpr = bg::Expr::id(ASTBoogieUtils::BOOGIE_MSG_VALUE);
		addTCC(m_currentExpr, make_shared<IntegerType>(256, IntegerType::Modifier::Unsigned));
		return false;
	}
	// array.length
	bool isArray = type->category() == Type::Category::Array;
	if (isArray && _node.memberName() == "length")
	{
		m_currentExpr = getArrayLength(expr, _node);
		return false;
	}
	// fixed size byte array length
	if (type->category() == Type::Category::FixedBytes && _node.memberName() == "length")
	{
		auto fbType = dynamic_cast<FixedBytesType const*>(&*_node.expression().annotation().type);
		m_currentExpr = bg::Expr::lit(fbType->numBytes());
		return false;
	}
	// Non-special member access: 'referencedDeclaration' should point to the
	// declaration corresponding to 'memberName'
	if (_node.annotation().referencedDeclaration == nullptr)
	{
		m_context.reportError(&_node, "Member without corresponding declaration (probably an unsupported special member)");
		m_currentExpr = bg::Expr::id(ASTBoogieUtils::ERR_EXPR);
		return false;
	}
	m_currentExpr = bg::Expr::id(ASTBoogieUtils::mapDeclName(*_node.annotation().referencedDeclaration));
	// Check for getter
	m_isGetter =  dynamic_cast<const VariableDeclaration*>(_node.annotation().referencedDeclaration);
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
					m_isLibraryCallStatic = true;
			}
		}
	}

	return false;
}

bool ASTBoogieExpressionConverter::visit(IndexAccess const& _node)
{
	_node.baseExpression().accept(*this);
	bg::Expr::Ref baseExpr = m_currentExpr;

	_node.indexExpression()->accept(*this); // TODO: can this be a nullptr?
	bg::Expr::Ref indexExpr = m_currentExpr;

	// Fixed size byte arrays
	if (_node.baseExpression().annotation().type->category() == Type::Category::FixedBytes)
	{
		auto fbType = dynamic_cast<FixedBytesType const*>(&*_node.baseExpression().annotation().type);
		unsigned fbSize = fbType->numBytes();

		// Check bounds (typechecked for unsigned, so >= 0)
		addSideEffect(bg::Stmt::assume(bg::Expr::gte(indexExpr, bg::Expr::lit((unsigned)0))));
		addSideEffect(bg::Stmt::assert_(bg::Expr::lt(indexExpr, bg::Expr::lit(fbSize)),
					ASTBoogieUtils::createAttrs(_node.location(), "Index may be out of bounds", *m_context.currentScanner())));

		// Do a case split on which slice to use
		for (unsigned i = 0; i < fbSize; ++ i)
		{
			bg::Expr::Ref slice = m_context.intSlice(baseExpr, fbSize*8, (i+1)*8 - 1, i*8);
			if (i == 0)
				m_currentExpr = slice;
			else
			{
				m_currentExpr = bg::Expr::if_then_else(
						bg::Expr::eq(indexExpr, bg::Expr::lit(i)),
						slice, m_currentExpr
				);
			}
		}
		return false;
	}

	if (_node.baseExpression().annotation().type->category() == Type::Category::Array && m_context.isBvEncoding())
	{
		indexExpr = ASTBoogieUtils::checkImplicitBvConversion(indexExpr, _node.indexExpression()->annotation().type, make_shared<IntegerType>(256), m_context);
	}
	// Index access is simply converted to a select in Boogie, which is fine
	// as long as it is not an LHS of an assignment (e.g., x[i] = v), but
	// that case is handled when converting assignments
	m_currentExpr = bg::Expr::sel(baseExpr, indexExpr);
	addTCC(m_currentExpr, _node.annotation().type);

	return false;
}

bool ASTBoogieExpressionConverter::visit(Identifier const& _node)
{
	if (_node.name() == ASTBoogieUtils::VERIFIER_SUM)
	{
		m_currentExpr = bg::Expr::id(ASTBoogieUtils::VERIFIER_SUM);
		return false;
	}

	if (!_node.annotation().referencedDeclaration)
	{
		m_context.reportError(&_node, "Identifier '" + _node.name() + "' has no matching declaration");
		m_currentExpr = bg::Expr::id(ASTBoogieUtils::ERR_EXPR);
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
	if (referencesStateVar) { m_currentExpr = bg::Expr::sel(declName, ASTBoogieUtils::BOOGIE_THIS); }
	// Other identifiers can be referenced by their name
	else { m_currentExpr = bg::Expr::id(declName); }

	addTCC(m_currentExpr, _node.annotation().referencedDeclaration->type());

	return false;
}

bool ASTBoogieExpressionConverter::visit(ElementaryTypeNameExpression const& _node)
{
	BOOST_THROW_EXCEPTION(InternalCompilerError() << errinfo_comment("Unhandled node: ElementaryTypeNameExpression") << errinfo_sourceLocation(_node.location()));
	return false;
}

bool ASTBoogieExpressionConverter::visit(Literal const& _node)
{
	TypePointer type = _node.annotation().type;
	Type::Category typeCategory = type->category();

	switch (typeCategory) {
	case Type::Category::RationalNumber: {
		auto rationalType = std::dynamic_pointer_cast<RationalNumberType const>(type);
		if (rationalType != nullptr) {
			// For now, just the integers
			if (!rationalType->isFractional()) {
				m_currentExpr = bg::Expr::lit(bg::bigint(_node.value()));
				return false;
			}
		}
		break;
	}
	case Type::Category::Bool:
		m_currentExpr = bg::Expr::lit(_node.value() == "true");
		return false;
	case Type::Category::Address: {
		string name = "address_" + _node.value();
		m_newConstants.push_back(bg::Decl::constant(name, ASTBoogieUtils::BOOGIE_ADDRESS_TYPE, true));
		m_currentExpr = bg::Expr::id(name);
		return false;
	}
	case Type::Category::StringLiteral: {
		string name = "literal_string#" + to_string(_node.id());
		m_newConstants.push_back(bg::Decl::constant(name, ASTBoogieUtils::BOOGIE_STRING_TYPE, true));
		m_currentExpr = bg::Expr::id(name);
		return false;
	}
	default: {
		// Report unsupported
		string tpStr = type->toString();
		m_context.reportError(&_node, "Unsupported literal for type " + tpStr.substr(0, tpStr.find(' ')));
		m_currentExpr = bg::Expr::id(ASTBoogieUtils::ERR_EXPR);
		break;
	}
	}

	return false;
}

bool ASTBoogieExpressionConverter::visitNode(ASTNode const& _node)
{
	BOOST_THROW_EXCEPTION(InternalCompilerError() << errinfo_comment("Unhandled node") << errinfo_sourceLocation(_node.location()));
	return true;
}

}
}
