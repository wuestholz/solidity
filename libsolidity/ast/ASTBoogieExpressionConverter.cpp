#include <boost/algorithm/string/predicate.hpp>
#include <libsolidity/ast/ASTBoogieExpressionConverter.h>
#include <libsolidity/ast/ASTBoogieUtils.h>
#include <libsolidity/ast/AST.h>

using namespace std;
using namespace dev;
using namespace dev::solidity;

namespace dev
{
namespace solidity
{

const smack::Expr* ASTBoogieExpressionConverter::getArrayLength(const smack::Expr* expr, ASTNode const& associatedNode)
{
	// Array is a local variable
	if (auto localArray = dynamic_cast<const smack::VarExpr*>(expr))
	{
		return smack::Expr::id(localArray->name() + ASTBoogieUtils::BOOGIE_LENGTH);
	}
	// Array is state variable
	if (auto stateArray = dynamic_cast<const smack::SelExpr*>(expr))
	{
		if (auto baseArray = dynamic_cast<const smack::VarExpr*>(stateArray->getBase()))
		{
			return smack::Expr::sel(
							smack::Expr::id(baseArray->name() + ASTBoogieUtils::BOOGIE_LENGTH),
							stateArray->getIdxs());
		}
	}

	m_context.reportError(&associatedNode, "Unsupported access to array length");
	return smack::Expr::id(ASTBoogieUtils::ERR_EXPR);
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
	m_context.reportError(node, "Unsupported identifier for sum function");
	return smack::Expr::id(ASTBoogieUtils::ERR_EXPR);
}

void ASTBoogieExpressionConverter::addTCC(smack::Expr const* expr, TypePointer tp)
{
	if (m_context.encoding() == BoogieContext::Encoding::MOD && ASTBoogieUtils::isBitPreciseType(tp))
	{
		unsigned bits = ASTBoogieUtils::getBits(tp);
		bool isSigned = ASTBoogieUtils::isSigned(tp);
		if (isSigned)
		{
			auto largestSigned = smack::Expr::lit(boost::multiprecision::pow(smack::bigint(2), bits - 1) - 1);
			auto smallestSigned = smack::Expr::lit(-boost::multiprecision::pow(smack::bigint(2), bits - 1));
			m_tccs.push_back(smack::Expr::and_(
					smack::Expr::lte(smallestSigned, expr),
					smack::Expr::lte(expr, largestSigned)));
		}
		else
		{
			auto largestUnsigned = smack::Expr::lit(boost::multiprecision::pow(smack::bigint(2), bits) - 1);
			auto smallestUnsigned = smack::Expr::lit(long(0));
			m_tccs.push_back(smack::Expr::and_(
					smack::Expr::lte(smallestUnsigned, expr),
					smack::Expr::lte(expr, largestUnsigned)));
		}

	}
}

void ASTBoogieExpressionConverter::addSideEffect(smack::Stmt const* stmt)
{
	for (auto oc : m_ocs)
	{
		m_newStatements.push_back(smack::Stmt::assign(
			smack::Expr::id(ASTBoogieUtils::VERIFIER_OVERFLOW),
			smack::Expr::or_(smack::Expr::id(ASTBoogieUtils::VERIFIER_OVERFLOW), smack::Expr::not_(oc))));
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

	unsigned bits = ASTBoogieUtils::getBits(_node.leftHandSide().annotation().type);
	bool isSigned = ASTBoogieUtils::isSigned(_node.leftHandSide().annotation().type);

	// Bit-precise mode
	if (m_context.isBvEncoding() && ASTBoogieUtils::isBitPreciseType(_node.leftHandSide().annotation().type))
	{
		// Check for implicit conversion
		rhs = ASTBoogieUtils::checkImplicitBvConversion(rhs, _node.rightHandSide().annotation().type, _node.leftHandSide().annotation().type, m_context);
	}
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
		m_context.reportError(&_node, string("Unsupported assignment operator: ") + Token::toString(_node.assignmentOperator()));
		m_currentExpr = smack::Expr::id(ASTBoogieUtils::ERR_EXPR);
		return false;
	}
	if (m_context.overflow() && result.second)
	{
		m_ocs.push_back(result.second);
	}
	// Create the assignment with the helper method
	createAssignment(_node.leftHandSide(), lhs, result.first);
	return false;
}

void ASTBoogieExpressionConverter::createAssignment(Expression const& originalLhs, smack::Expr const *lhs, smack::Expr const* rhs)
{
	// First check if shadow variables need to be updated
	if (auto lhsIdx = dynamic_cast<IndexAccess const*>(&originalLhs))
	{
		if (auto lhsId = dynamic_cast<Identifier const*>(&lhsIdx->baseExpression()))
		{
			if (m_context.currentSumDecls()[lhsId->annotation().referencedDeclaration])
			{
				// arr[i] = x becomes arr#sum := arr#sum[this := (arr#sum[this] + x - arr[i])]
				auto sumId = smack::Expr::id(ASTBoogieUtils::mapDeclName(*lhsId->annotation().referencedDeclaration) + ASTBoogieUtils::BOOGIE_SUM);

				unsigned bits = ASTBoogieUtils::getBits(originalLhs.annotation().type);
				bool isSigned = ASTBoogieUtils::isSigned(originalLhs.annotation().type);

				auto selExpr = smack::Expr::sel(sumId, smack::Expr::id(ASTBoogieUtils::BOOGIE_THIS));
				auto subResult = ASTBoogieUtils::encodeArithBinaryOp(m_context, nullptr, Token::Value::Sub, rhs, lhs, bits, isSigned);
				auto updResult = ASTBoogieUtils::encodeArithBinaryOp(m_context, nullptr, Token::Value::Add, selExpr, subResult.first, bits, isSigned);

				addSideEffect(smack::Stmt::assign(
						sumId,
						smack::Expr::upd(sumId, smack::Expr::id(ASTBoogieUtils::BOOGIE_THIS), updResult.first)));
			}
		}
	}


	// If LHS is simply an identifier, we can assign to it
	if (dynamic_cast<smack::VarExpr const*>(lhs))
	{
		addSideEffect(smack::Stmt::assign(lhs, rhs));
		return;
	}

	// If LHS is an indexer (arrays/maps), it needs to be transformed to an update
	if (auto lhsSel = dynamic_cast<const smack::SelExpr*>(lhs))
	{
		if (auto lhsUpd = dynamic_cast<const smack::UpdExpr*>(selectToUpdate(lhsSel, rhs)))
		{
			addSideEffect(smack::Stmt::assign(lhsUpd->getBase(), lhsUpd));
			return;
		}
	}

	m_context.reportError(&originalLhs, "Unsupported assignment (LHS must be identifier/indexer)");
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
		m_context.reportError(&_node, "Tuples are not supported");
		m_currentExpr = smack::Expr::id(ASTBoogieUtils::ERR_EXPR);
	}
	return false;
}

bool ASTBoogieExpressionConverter::visit(UnaryOperation const& _node)
{
	// Check if constant propagation could infer the result
	string tpStr = _node.annotation().type->toString();
	if (boost::starts_with(tpStr, "int_const"))
	{
		m_currentExpr = smack::Expr::lit(smack::bigint(tpStr.substr(10)));
		return false;
	}

	// Get operand recursively
	_node.subExpression().accept(*this);
	const smack::Expr* subExpr = m_currentExpr;

	unsigned bits = ASTBoogieUtils::getBits(_node.annotation().type);
	bool isSigned = ASTBoogieUtils::isSigned(_node.annotation().type);

	switch (_node.getOperator()) {
	case Token::Add: m_currentExpr = subExpr; break; // Unary plus does not do anything
	case Token::Not: m_currentExpr = smack::Expr::not_(subExpr); break;

	case Token::Sub:
	case Token::BitNot: {
		auto exprResult = ASTBoogieUtils::encodeArithUnaryOp(m_context, &_node, _node.getOperator(), subExpr, bits, isSigned);
		m_currentExpr = exprResult.first;
		if (m_context.overflow() && exprResult.second){
			m_ocs.push_back(exprResult.second);
		}
		break;
	}

	// Inc and Dec shares most part of the code
	case Token::Inc:
	case Token::Dec:
		{
			const smack::Expr* lhs = subExpr;
			auto rhsResult =
					ASTBoogieUtils::encodeArithBinaryOp(m_context, &_node,
							_node.getOperator() == Token::Inc ? Token::Add : Token::Sub,
							lhs,
							m_context.isBvEncoding() ? smack::Expr::lit(smack::bigint(1), bits) : smack::Expr::lit(smack::bigint(1)),
							bits, isSigned);
			smack::Decl* tempVar = smack::Decl::variable("inc#" + to_string(_node.id()),
					ASTBoogieUtils::mapType(_node.subExpression().annotation().type, _node, m_context));
			m_newDecls.push_back(tempVar);
			if (_node.isPrefixOperation()) // ++x (or --x)
			{
				// First do the assignment x := x + 1 (or x := x - 1)
				if (m_context.overflow() && rhsResult.second) { m_ocs.push_back(rhsResult.second); }
				createAssignment(_node.subExpression(), lhs, rhsResult.first);
				// Then the assignment tmp := x
				addSideEffect(smack::Stmt::assign(smack::Expr::id(tempVar->getName()), lhs));
			}
			else // x++ (or x--)
			{
				// First do the assignment tmp := x
				addSideEffect(smack::Stmt::assign(smack::Expr::id(tempVar->getName()), subExpr));
				// Then the assignment x := x + 1 (or x := x - 1)
				if (m_context.overflow() && rhsResult.second) { m_ocs.push_back(rhsResult.second); }
				createAssignment(_node.subExpression(), lhs, rhsResult.first);
			}
			// Result is the tmp variable (if the assignment is part of an expression)
			m_currentExpr = smack::Expr::id(tempVar->getName());
		}
		break;
	default:
		m_context.reportError(&_node, string("Unsupported unary operator: ") + Token::toString(_node.getOperator()));
		m_currentExpr = smack::Expr::id(ASTBoogieUtils::ERR_EXPR);
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
		m_currentExpr = smack::Expr::lit(smack::bigint(tpStr.substr(10)));
		return false;
	}

	// Get lhs recursively
	_node.leftExpression().accept(*this);
	const smack::Expr* lhs = m_currentExpr;

	// Get rhs recursively
	_node.rightExpression().accept(*this);
	const smack::Expr* rhs = m_currentExpr;

	// Common type might not be equal to the type of the node, e.g., in case of uint32 == uint64,
	// the common type is uint64, but the type of the node is bool
	auto commonType = _node.leftExpression().annotation().type->binaryOperatorResult(_node.getOperator(), _node.rightExpression().annotation().type);

	unsigned bits = ASTBoogieUtils::getBits(commonType);
	bool isSigned = ASTBoogieUtils::isSigned(commonType);

	// Check implicit conversion for bitvectors
	if (m_context.isBvEncoding() && ASTBoogieUtils::isBitPreciseType(commonType))
	{
		lhs = ASTBoogieUtils::checkImplicitBvConversion(lhs, _node.leftExpression().annotation().type, commonType, m_context);
		rhs = ASTBoogieUtils::checkImplicitBvConversion(rhs, _node.rightExpression().annotation().type, commonType, m_context);
	}

	switch(_node.getOperator())
	{
	// Non-arithmetic operations
	case Token::And: m_currentExpr = smack::Expr::and_(lhs, rhs); break;
	case Token::Or: m_currentExpr = smack::Expr::or_(lhs, rhs); break;

	// Arithmetic operations
	case Token::Add:
	case Token::Sub:
	case Token::Mul:
	case Token::Div:
	case Token::Mod:
	case Token::Exp:

	case Token::Equal:
	case Token::NotEqual:
	case Token::LessThan:
	case Token::GreaterThan:
	case Token::LessThanOrEqual:
	case Token::GreaterThanOrEqual:

	case Token::BitAnd:
	case Token::BitOr:
	case Token::BitXor:
	case Token::SHL:
	case Token::SAR: {
		auto exprResult = ASTBoogieUtils::encodeArithBinaryOp(m_context, &_node, _node.getOperator(), lhs, rhs, bits, isSigned);
		m_currentExpr = exprResult.first;
		if (m_context.overflow() && exprResult.second){
			m_ocs.push_back(exprResult.second);
		}
		break;
	}

	default:
		m_context.reportError(&_node, string("Unsupported binary operator ") + Token::toString(_node.getOperator()));
		m_currentExpr = smack::Expr::id(ASTBoogieUtils::ERR_EXPR);
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
						m_context.reportError(&_node, "Unsupported conversion to address");
					}
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
		m_currentExpr = smack::Expr::id(ASTBoogieUtils::ERR_EXPR);
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
	m_currentAddress = smack::Expr::id(ASTBoogieUtils::BOOGIE_THIS);
	m_currentMsgValue = nullptr;
	m_isGetter = false;
	m_isLibraryCall = false;
	// Process expression
	_node.expression().accept(*this);

	// 'm_currentExpr' should be an identifier, giving the name of the function
	string funcName;
	if (auto varExpr = dynamic_cast<smack::VarExpr const*>(m_currentExpr))
	{
		funcName = varExpr->name();
	}
	else
	{
		m_context.reportError(&_node, "Only identifiers are supported as function calls");
		funcName = ASTBoogieUtils::ERR_EXPR;
	}

	if (m_isGetter && _node.arguments().size() > 0)
	{
		m_context.reportError(&_node, "Getter arguments are not supported");
	}

	// Process arguments recursively
	list<const smack::Expr*> args;
	// First, pass extra arguments
	if (m_isLibraryCall) { args.push_back(smack::Expr::id(ASTBoogieUtils::BOOGIE_THIS)); } // this
	else { args.push_back(m_currentAddress); } // this
	args.push_back(smack::Expr::id(ASTBoogieUtils::BOOGIE_THIS)); // msg.sender
	smack::Expr const* defaultMsgValue = (m_context.isBvEncoding() ?
			smack::Expr::lit(smack::bigint(0), 256) : smack::Expr::lit(smack::bigint(0)));
	args.push_back(m_currentMsgValue ? m_currentMsgValue : defaultMsgValue); // msg.value
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
			{
				m_currentExpr = ASTBoogieUtils::checkImplicitBvConversion(m_currentExpr, arg->annotation().type, calledFunc->parameters()[i]->annotation().type, m_context);
			}
		}

		// Do not add argument for call
		if (funcName != ASTBoogieUtils::BOOGIE_CALL)
		{
			args.push_back(m_currentExpr);
			// Array lenght
			if (arg->annotation().type && arg->annotation().type->category() == Type::Category::Array)
			{
				args.push_back(getArrayLength(m_currentExpr, *arg));
			}
		}
	}

	// Check for calls to special functions

	// Assert is a separate statement in Boogie (instead of a function call)
	if (funcName == ASTBoogieUtils::SOLIDITY_ASSERT)
	{
		if (_node.arguments().size() != 1)
		{
			BOOST_THROW_EXCEPTION(InternalCompilerError() <<
							errinfo_comment("Assert should have exactly one argument") <<
							errinfo_sourceLocation(_node.location()));
		}
		// Parameter of assert is the first (and only) normal argument
		list<const smack::Expr*>::iterator it = args.begin();
		std::advance(it, args.size() - _node.arguments().size());
		addSideEffect(smack::Stmt::assert_(*it, ASTBoogieUtils::createAttrs(_node.location(), "Assertion might not hold.", *m_context.currentScanner())));
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
		// Parameter of assume is the first normal argument (second is optional message omitted in Boogie)
		list<const smack::Expr*>::iterator it = args.begin();
		std::advance(it, args.size() - _node.arguments().size());
		addSideEffect(smack::Stmt::assume(*it));
		return false;
	}

	// Revert is mapped to assume(false) statement in Boogie (instead of a function call)
	// Its argument is an optional message, omitted in Boogie
	if (funcName == ASTBoogieUtils::SOLIDITY_REVERT)
	{
		if (_node.arguments().size() > 1)
		{
			BOOST_THROW_EXCEPTION(InternalCompilerError() <<
							errinfo_comment("Revert should have at most one argument") <<
							errinfo_sourceLocation(_node.location()));
		}
		addSideEffect(smack::Stmt::assume(smack::Expr::lit(false)));
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
			{
				magicVar = dynamic_cast<MagicVariableDeclaration const *>(exprId->annotation().referencedDeclaration);
			}

			if (magicVar)
			{
				m_context.currentSumDecls()[sumDecl] = dynamic_cast<FunctionType const*>(&*magicVar->type())->returnParameterTypes()[0];
				tp = m_context.currentSumDecls()[sumDecl];
			}
			else { m_context.reportError(&_node, "Could not find sum function"); }

			auto declCategory = id->annotation().type->category();
			if (declCategory != Type::Category::Mapping && declCategory != Type::Category::Array)
			{
				m_context.reportError(&_node, "Argument of sum must be an array or a mapping");
			}
		}
		else
		{
			m_context.reportError(&_node, "Argument of sum must be an identifier");
		}
		m_currentExpr = getSumShadowVar(&*_node.arguments()[0]);
		addTCC(m_currentExpr, tp);
		return false;
	}

	// If msg.value was set, we should reduce our own balance (and the called function will increase its own)
	if (m_currentMsgValue)
	{
		// assert(balance[this] >= msg.value)
		auto selExpr = smack::Expr::sel(ASTBoogieUtils::BOOGIE_BALANCE, ASTBoogieUtils::BOOGIE_THIS);
		auto geqResult = ASTBoogieUtils::encodeArithBinaryOp(m_context, nullptr, Token::Value::GreaterThanOrEqual, selExpr, m_currentMsgValue, 256, false);
		addSideEffect(smack::Stmt::assert_(geqResult.first,
				ASTBoogieUtils::createAttrs(_node.location(), "Calling payable function might fail due to insufficient ether", *m_context.currentScanner())));
		// balance[this] -= msg.value
		auto subResult = ASTBoogieUtils::encodeArithBinaryOp(m_context, nullptr, Token::Value::Sub,
												smack::Expr::sel(ASTBoogieUtils::BOOGIE_BALANCE, ASTBoogieUtils::BOOGIE_THIS),
												m_currentMsgValue, 256, false);
		addSideEffect(smack::Stmt::assign(
				smack::Expr::id(ASTBoogieUtils::BOOGIE_BALANCE),
				smack::Expr::upd(
						smack::Expr::id(ASTBoogieUtils::BOOGIE_BALANCE),
						smack::Expr::id(ASTBoogieUtils::BOOGIE_THIS),
						subResult.first)));
	}

	// External calls require the invariants to hold
	if (funcName == ASTBoogieUtils::BOOGIE_CALL)
	{
		for (auto invar : m_context.currentContractInvars())
		{
			for (auto tcc : invar.tccs) { addSideEffect(smack::Stmt::assert_(tcc,
										ASTBoogieUtils::createAttrs(_node.location(), "Variables for invariant '" + invar.exprStr + "' might be out of range before external call.", *m_context.currentScanner()))); }
			addSideEffect(smack::Stmt::assert_(invar.expr,
					ASTBoogieUtils::createAttrs(_node.location(), "Invariant '" + invar.exprStr + "' might not hold before external call.", *m_context.currentScanner())));
		}
	}

	// TODO: check for void return in a more appropriate way
	if (_node.annotation().type->toString() != "tuple()")
	{
		// Create fresh variable to store the result
		smack::Decl* returnVar = smack::Decl::variable(
				funcName + "#" + to_string(_node.id()),
				ASTBoogieUtils::mapType(_node.annotation().type, _node, m_context));
		m_newDecls.push_back(returnVar);
		// Result of the function call is the fresh variable
		m_currentExpr = smack::Expr::id(returnVar->getName());

		if (m_isGetter)
		{
			// Getters are replaced with map access (instead of function call)
			addSideEffect(smack::Stmt::assign(
					smack::Expr::id(returnVar->getName()),
					smack::Expr::sel(smack::Expr::id(funcName), m_currentAddress)));
		}
		else
		{
			// Assign call to the fresh variable
			addSideEffect(smack::Stmt::annot(ASTBoogieUtils::createAttrs(_node.location(), "", *m_context.currentScanner())));
			addSideEffect(smack::Stmt::call(funcName, args, {returnVar->getName()}));

			// The call function is special as it indicates failure in a return value and in this case
			// we must undo reducing our balance
			if (funcName == ASTBoogieUtils::BOOGIE_CALL && m_currentMsgValue)
			{
				smack::Block* revert = smack::Block::block();
				// balance[this] += msg.value
				auto addResult = ASTBoogieUtils::encodeArithBinaryOp(m_context, nullptr, Token::Value::Add,
															smack::Expr::sel(ASTBoogieUtils::BOOGIE_BALANCE, ASTBoogieUtils::BOOGIE_THIS),
															m_currentMsgValue, 256, false);
				revert->addStmt(smack::Stmt::assign(
						smack::Expr::id(ASTBoogieUtils::BOOGIE_BALANCE),
						smack::Expr::upd(
								smack::Expr::id(ASTBoogieUtils::BOOGIE_BALANCE),
								smack::Expr::id(ASTBoogieUtils::BOOGIE_THIS),
								addResult.first)));
				addSideEffect(smack::Stmt::ifelse(smack::Expr::not_(m_currentExpr), revert));
			}
		}

	}
	else // No return value for function
	{
		m_currentExpr = nullptr;
		addSideEffect(smack::Stmt::annot(ASTBoogieUtils::createAttrs(_node.location(), "", *m_context.currentScanner())));
		addSideEffect(smack::Stmt::call(funcName, args));
	}

	// Assume invariants after external call
	if (funcName == ASTBoogieUtils::BOOGIE_CALL)
	{

		for (auto invar : m_context.currentContractInvars()) {
			for (auto tcc : invar.tccs) { addSideEffect(smack::Stmt::assume(tcc)); }
			addSideEffect(smack::Stmt::assume(invar.expr));
		}
	}

	return false;
}

bool ASTBoogieExpressionConverter::visit(NewExpression const& _node)
{
	m_context.reportError(&_node, "New expression is not supported");
	m_currentExpr = smack::Expr::id(ASTBoogieUtils::ERR_EXPR);
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
	// If we are accessing something on 'super', the current address should be 'this'
	// and not 'super', because that does not exist
	if (auto id = dynamic_cast<Identifier const*>(&_node.expression()))
	{
		if (id->annotation().referencedDeclaration->name() == ASTBoogieUtils::SOLIDITY_SUPER)
		{
			m_currentAddress = smack::Expr::id(ASTBoogieUtils::BOOGIE_THIS);
		}
	}
	// Type of the expression
	string typeStr = _node.expression().annotation().type->toString();

	// Handle special members/functions

	// address.balance / this.balance
	if (_node.memberName() == ASTBoogieUtils::SOLIDITY_BALANCE)
	{
		if (typeStr == ASTBoogieUtils::SOLIDITY_ADDRESS_TYPE)
		{
			m_currentExpr = smack::Expr::sel(smack::Expr::id(ASTBoogieUtils::BOOGIE_BALANCE), expr);
			addTCC(m_currentExpr, make_shared<IntegerType>(256, IntegerType::Modifier::Unsigned));
			return false;
		}
		if (auto exprId = dynamic_cast<Identifier const*>(&_node.expression()))
		{
			if (exprId->name() == ASTBoogieUtils::SOLIDITY_THIS)
			{
				m_currentExpr = smack::Expr::sel(ASTBoogieUtils::BOOGIE_BALANCE, ASTBoogieUtils::BOOGIE_THIS);
				addTCC(m_currentExpr, make_shared<IntegerType>(256, IntegerType::Modifier::Unsigned));
				return false;
			}
		}
	}
	// address.transfer()
	if (typeStr == ASTBoogieUtils::SOLIDITY_ADDRESS_TYPE && _node.memberName() == ASTBoogieUtils::SOLIDITY_TRANSFER)
	{
		m_context.includeTransferFunction();
		m_currentExpr = smack::Expr::id(ASTBoogieUtils::BOOGIE_TRANSFER);
		return false;
	}
	// address.send()
	if (typeStr == ASTBoogieUtils::SOLIDITY_ADDRESS_TYPE && _node.memberName() == ASTBoogieUtils::SOLIDITY_SEND)
	{
		m_context.includeSendFunction();
		m_currentExpr = smack::Expr::id(ASTBoogieUtils::BOOGIE_SEND);
		return false;
	}
	// address.call()
	if (typeStr == ASTBoogieUtils::SOLIDITY_ADDRESS_TYPE && _node.memberName() == ASTBoogieUtils::SOLIDITY_CALL)
	{
		m_context.includeCallFunction();
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
		addTCC(m_currentExpr, make_shared<IntegerType>(256, IntegerType::Modifier::Unsigned));
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
		m_context.reportError(&_node, "Member without corresponding declaration (probably an unsupported special member)");
		m_currentExpr = smack::Expr::id(ASTBoogieUtils::ERR_EXPR);
		return false;
	}
	m_currentExpr = smack::Expr::id(ASTBoogieUtils::mapDeclName(*_node.annotation().referencedDeclaration));
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
			addSideEffect(smack::Stmt::assert_(smack::Expr::eq(indexExpr, smack::Expr::lit((unsigned)0)),
					ASTBoogieUtils::createAttrs(_node.location(), "Index may be out of bounds", *m_context.currentScanner())));
			m_currentExpr = baseExpr;
			// TODO: TCC?
			return false;
		}
	}
	if (_node.baseExpression().annotation().type->category() == Type::Category::Array && m_context.isBvEncoding())
	{
		indexExpr = ASTBoogieUtils::checkImplicitBvConversion(indexExpr, _node.indexExpression()->annotation().type, make_shared<IntegerType>(256), m_context);
	}
	// Index access is simply converted to a select in Boogie, which is fine
	// as long as it is not an LHS of an assignment (e.g., x[i] = v), but
	// that case is handled when converting assignments
	m_currentExpr = smack::Expr::sel(baseExpr, indexExpr);
	addTCC(m_currentExpr, _node.annotation().type);

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
		m_context.reportError(&_node, "Identifier '" + _node.name() + "' has no matching declaration");
		m_currentExpr = smack::Expr::id(ASTBoogieUtils::ERR_EXPR);
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

	m_context.reportError(&_node, "Unsupported literal for type " + tpStr.substr(0, tpStr.find(' ')));
	m_currentExpr = smack::Expr::id(ASTBoogieUtils::ERR_EXPR);
	return false;
}

bool ASTBoogieExpressionConverter::visitNode(ASTNode const& _node)
{
	BOOST_THROW_EXCEPTION(InternalCompilerError() << errinfo_comment("Unhandled node") << errinfo_sourceLocation(_node.location()));
	return true;
}

}
}
