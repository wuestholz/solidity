#include <boost/algorithm/string/predicate.hpp>
#include <libsolidity/ast/ASTBoogieExpressionConverter.h>
#include <libsolidity/ast/ASTBoogieUtils.h>
#include <libsolidity/ast/AST.h>
#include <libsolidity/ast/TypeProvider.h>
#include <liblangutil/Exceptions.h>

using namespace std;
using namespace dev;
using namespace dev::solidity;
using namespace langutil;
using namespace boogie;

namespace bg = boogie;

namespace dev
{
namespace solidity
{

Expr::Ref ASTBoogieExpressionConverter::getArrayLength(Expr::Ref expr, ASTNode const& associatedNode)
{
	// Array is a local variable
	if (auto localArray = dynamic_pointer_cast<bg::VarExpr const>(expr))
		return Expr::id(localArray->name() + ASTBoogieUtils::BOOGIE_LENGTH);
	// Array is state variable
	if (auto stateArray = dynamic_pointer_cast<bg::SelExpr const>(expr))
		if (auto baseArray = dynamic_pointer_cast<bg::VarExpr const>(stateArray->getBase()))
		{
			return Expr::sel(
							Expr::id(baseArray->name() + ASTBoogieUtils::BOOGIE_LENGTH),
							stateArray->getIdxs());
		}

	m_context.reportError(&associatedNode, "Unsupported access to array length");
	return Expr::id(ASTBoogieUtils::ERR_EXPR);
}

Expr::Ref ASTBoogieExpressionConverter::getSumShadowVar(ASTNode const* node)
{
	if (auto sumBase = dynamic_cast<Identifier const*>(node))
		if (auto sumBaseDecl = sumBase->annotation().referencedDeclaration)
		{
			return Expr::sel(
					Expr::id(m_context.mapDeclName(*sumBaseDecl) + ASTBoogieUtils::BOOGIE_SUM),
					m_context.boogieThis());
		}

	m_context.reportError(node, "Unsupported identifier for sum function");
	return Expr::id(ASTBoogieUtils::ERR_EXPR);
}

void ASTBoogieExpressionConverter::addTCC(Expr::Ref expr, TypePointer tp)
{
	if (m_context.encoding() == BoogieContext::Encoding::MOD && ASTBoogieUtils::isBitPreciseType(tp))
		m_tccs.push_back(ASTBoogieUtils::getTCCforExpr(expr, tp));
}

void ASTBoogieExpressionConverter::addSideEffect(Stmt::Ref stmt)
{
	for (auto oc : m_ocs)
	{
		m_newStatements.push_back(Stmt::assign(
			Expr::id(ASTBoogieUtils::VERIFIER_OVERFLOW),
			Expr::or_(Expr::id(ASTBoogieUtils::VERIFIER_OVERFLOW), Expr::not_(oc))));
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

ASTBoogieExpressionConverter::Result ASTBoogieExpressionConverter::convert(Expression const& _node)
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
	Expr::Ref cond = m_currentExpr;

	// Get true expression recursively
	_node.trueExpression().accept(*this);
	Expr::Ref trueExpr = m_currentExpr;

	// Get false expression recursively
	_node.falseExpression().accept(*this);
	Expr::Ref falseExpr = m_currentExpr;

	m_currentExpr = Expr::cond(cond, trueExpr, falseExpr);
	return false;
}

bool ASTBoogieExpressionConverter::visit(Assignment const& _node)
{
	ASTBoogieUtils::ExprWithCC result;

	// LHS/RHS Nodes
	Expression const& lhsNode = _node.leftHandSide();
	Expression const& rhsNode = _node.rightHandSide();

	// LHS/RHS Types
	TypePointer lhsType = lhsNode.annotation().type;
	TypePointer rhsType = rhsNode.annotation().type;

	// What kind of assignment
	Token assignType = _node.assignmentOperator();

	// Get lhs recursively
	_node.leftHandSide().accept(*this);
	Expr::Ref lhsExpr = m_currentExpr;

	// Get rhs recursively
	_node.rightHandSide().accept(*this);
	Expr::Ref rhsExpr = m_currentExpr;

	// Structs
	if (lhsType->category() == Type::Category::Struct)
	{
		solAssert(rhsType->category() == Type::Category::Struct, "LHS is struct but RHS is not");
		createStructAssignment(_node, lhsExpr, rhsExpr);
		return false;
	}

	// Bit-precise mode
	if (m_context.isBvEncoding() && ASTBoogieUtils::isBitPreciseType(lhsType))
		// Check for implicit conversion
		rhsExpr = ASTBoogieUtils::checkImplicitBvConversion(rhsExpr, rhsType, lhsType, m_context);

	// Check for additional arithmetic needed
	if (assignType == Token::Assign)
	{
		// rhs already contains the result
		result.expr = rhsExpr;
	}
	else
	{
		// Transform rhs based on the operator, e.g., a += b becomes a := bvadd(a, b)
		unsigned bits = ASTBoogieUtils::getBits(lhsType);
		bool isSigned = ASTBoogieUtils::isSigned(lhsType);
		result = ASTBoogieUtils::encodeArithBinaryOp(m_context, &_node, assignType, lhsExpr, rhsExpr, bits, isSigned);
	}

	if (m_context.overflow() && result.cc)
		m_ocs.push_back(result.cc);

	// Create the assignment with the helper method
	createAssignment(_node.leftHandSide(), lhsExpr, result.expr);

	// Result will be the LHS (for chained assignments like x = y = 5)
	m_currentExpr = lhsExpr;

	return false;
}

void ASTBoogieExpressionConverter::createAssignment(Expression const& originalLhs, Expr::Ref lhs, Expr::Ref rhs)
{
	// If tuple, do element-wise
	if (auto lhsTuple = std::dynamic_pointer_cast<bg::TupleExpr const>(lhs))
	{
		auto rhsTuple = std::dynamic_pointer_cast<bg::TupleExpr const>(rhs);
		auto const& lhsElements = lhsTuple->elements();
		auto const& rhsElements = rhsTuple->elements();
		for (unsigned i = 0; i < lhsElements.size(); ++ i)
		{
			if (lhsElements[i])
				createAssignment(originalLhs, lhsElements[i], rhsElements[i]);
		}
		return;
	}

	// Check if ghost variables need to be updated
	if (auto lhsIdx = dynamic_cast<IndexAccess const*>(&originalLhs))
	{
		if (auto lhsId = dynamic_cast<Identifier const*>(&lhsIdx->baseExpression()))
		{
			if (m_context.currentSumDecls()[lhsId->annotation().referencedDeclaration])
			{
				// arr[i] = x becomes arr#sum := arr#sum[this := ((arr#sum[this] - arr[i]) + x)]
				auto sumId = Expr::id(m_context.mapDeclName(*lhsId->annotation().referencedDeclaration) + ASTBoogieUtils::BOOGIE_SUM);

				unsigned bits = ASTBoogieUtils::getBits(originalLhs.annotation().type);
				bool isSigned = ASTBoogieUtils::isSigned(originalLhs.annotation().type);

				auto selExpr = Expr::sel(sumId, m_context.boogieThis());
				auto subResult = ASTBoogieUtils::encodeArithBinaryOp(m_context, nullptr, Token::Sub, selExpr, lhs, bits, isSigned);
				auto updResult = ASTBoogieUtils::encodeArithBinaryOp(m_context, nullptr, Token::Add, subResult.expr, rhs, bits, isSigned);
				if (m_context.overflow())
				{
					addSideEffect(Stmt::comment("Implicit assumption that unsigned sum cannot underflow."));
					addSideEffect(Stmt::assume(subResult.cc));
				}
				addSideEffect(Stmt::assign(
						sumId,
						Expr::upd(sumId, m_context.boogieThis(), updResult.expr)));

			}
		}
	}


	// If LHS is simply an identifier, we can assign to it
	if (dynamic_pointer_cast<bg::VarExpr const>(lhs))
	{
		solAssert(dynamic_pointer_cast<bg::TupleExpr const>(rhs) == nullptr, "");
		addSideEffect(Stmt::assign(lhs, rhs));
		return;
	}

	// If LHS is an indexer (arrays/maps), it needs to be transformed to an update
	if (auto lhsSel = dynamic_pointer_cast<bg::SelExpr const>(lhs))
	{
		if (auto lhsUpd = dynamic_pointer_cast<bg::UpdExpr const>(selectToUpdate(lhsSel, rhs)))
		{
			addSideEffect(Stmt::assign(lhsUpd->getBase(), lhsUpd));
			return;
		}
	}

	m_context.reportError(&originalLhs, "Unsupported assignment (LHS must be identifier/indexer)");
}

void ASTBoogieExpressionConverter::createStructAssignment(Assignment const& _node, Expr::Ref lhsExpr, Expr::Ref rhsExpr)
{
	auto lhsStructType = dynamic_cast<StructType const*>(_node.leftHandSide().annotation().type);
	auto rhsStructType = dynamic_cast<StructType const*>(_node.rightHandSide().annotation().type);
	// LHS is memory
	if (lhsStructType->location() == DataLocation::Memory)
	{
		// RHS is memory --> reference copy
		if (rhsStructType->location() == DataLocation::Memory)
		{
			createAssignment(_node.leftHandSide(), lhsExpr, rhsExpr);
			m_currentExpr = lhsExpr;
			return;
		}
		// RHS is storage --> create new, deep copy
		else if (rhsStructType->location() == DataLocation::Storage)
		{
			// Create new
			auto varDecl = newStruct(&lhsStructType->structDefinition(), toString(_node.id()));
			addSideEffect(Stmt::assign(lhsExpr, Expr::id(varDecl->getName())));

			// Make deep copy
			deepCopyStruct(_node, &lhsStructType->structDefinition(), lhsExpr, rhsExpr,
					lhsStructType->location(), rhsStructType->location());
			m_currentExpr = lhsExpr;
			return;
		}
		else
		{
			m_context.reportError(&_node, "Unsupported assignment to memory struct");
			m_currentExpr = Expr::id(ASTBoogieUtils::ERR_EXPR);
			return;
		}
	}
	// LHS is storage
	else if (lhsStructType->location() == DataLocation::Storage)
	{
		// RHS is storage
		if (rhsStructType->location() == DataLocation::Storage)
		{
			// LHS is local storage --> reference copy
			if (lhsStructType->isPointer())
			{
				createAssignment(_node.leftHandSide(), lhsExpr, rhsExpr);
				m_currentExpr = lhsExpr;
				return;
			}
			// LHS is storage --> deep copy
			else
			{
				deepCopyStruct(_node, &lhsStructType->structDefinition(), lhsExpr, rhsExpr,
						lhsStructType->location(), rhsStructType->location());
				m_currentExpr = lhsExpr;
				return;
			}
		}
		// RHS is memory --> deep copy
		else if (rhsStructType->location() == DataLocation::Memory)
		{
			deepCopyStruct(_node, &lhsStructType->structDefinition(), lhsExpr, rhsExpr,
					lhsStructType->location(), rhsStructType->location());
			m_currentExpr = lhsExpr;
			return;
		}
		else
		{
			m_context.reportError(&_node, "Unsupported assignment to storage struct");
			m_currentExpr = Expr::id(ASTBoogieUtils::ERR_EXPR);
			return;
		}
	}

	m_context.reportError(&_node, "Unsupported kind of struct as LHS");
	m_currentExpr = Expr::id(ASTBoogieUtils::ERR_EXPR);
}

void ASTBoogieExpressionConverter::deepCopyStruct(Assignment const& _node, StructDefinition const* structDef,
		Expr::Ref lhsBase, Expr::Ref rhsBase, DataLocation lhsLoc, DataLocation rhsLoc)
{
	addSideEffect(Stmt::comment("Deep copy struct " + structDef->name()));
	// Loop through each member
	for (auto member : structDef->members())
	{
		// Get the mapping for the member
		Expr::Ref lhsMemberMapping = Expr::id(m_context.mapStructMemberName(*member, lhsLoc));
		Expr::Ref rhsMemberMapping = Expr::id(m_context.mapStructMemberName(*member, rhsLoc));
		// Select the lhs and rhs from the mapping
		auto lhsSel = dynamic_pointer_cast<SelExpr const>(Expr::sel(lhsMemberMapping, lhsBase));
		auto rhsSel = Expr::sel(rhsMemberMapping, rhsBase);

		auto memberTypeCat = member->annotation().type->category();
		// For nested structs do recursion
		if (memberTypeCat == Type::Category::Struct)
		{
			auto memberStructType = dynamic_cast<StructType const*>(member->annotation().type);
			// Deep copy into memory creates new
			if (lhsLoc == DataLocation::Memory)
			{
				// Create new
				auto varDecl = newStruct(&memberStructType->structDefinition(), toString(_node.id()) + toString(member->id()));
				// Update member to point to new
				auto lhsUpd = dynamic_pointer_cast<bg::UpdExpr const>(selectToUpdate(lhsSel, Expr::id(varDecl->getName())));
				addSideEffect(Stmt::assign(lhsUpd->getBase(), lhsUpd));
			}
			// Do the deep copy
			deepCopyStruct(_node, &memberStructType->structDefinition(), lhsSel, rhsSel, lhsLoc, rhsLoc);
		}
		// Unsupported stuff
		else if (memberTypeCat == Type::Category::Array || memberTypeCat == Type::Category::Mapping)
		{
			m_context.reportError(&_node, "Deep copy assignment between struct with arrays/mappings are not supported");
		}
		// For other types make the copy by updating the LHS with RHS
		else
		{
			auto lhsUpd = dynamic_pointer_cast<bg::UpdExpr const>(selectToUpdate(lhsSel, rhsSel));
			addSideEffect(Stmt::assign(lhsUpd->getBase(), lhsUpd));
		}
	}
}

Expr::Ref ASTBoogieExpressionConverter::selectToUpdate(std::shared_ptr<bg::SelExpr const> sel, Expr::Ref value)
{
	if (auto base = dynamic_pointer_cast<bg::SelExpr const>(sel->getBase()))
		return selectToUpdate(base, Expr::upd(base, sel->getIdxs(), value));
	else
		return Expr::upd(sel->getBase(), sel->getIdxs(), value);
}

bool ASTBoogieExpressionConverter::visit(TupleExpression const& _node)
{
	// Get the elements
	vector<Expr::Ref> elements;
	for (auto element : _node.components())
	{
		if (element != nullptr)
		{
			element->accept(*this);
			elements.push_back(m_currentExpr);
		}
		else
			elements.push_back(nullptr);
	}

	// Make the expression (tuples of size 1, just use the expression)
	if (elements.size() == 1)
		m_currentExpr = elements.back();
	else
		m_currentExpr = Expr::tuple(elements);

	return false;
}

bool ASTBoogieExpressionConverter::visit(UnaryOperation const& _node)
{
	// Check if constant propagation could infer the result
	string tpStr = _node.annotation().type->toString();
	if (boost::starts_with(tpStr, "int_const"))
	{
		m_currentExpr = Expr::lit(bg::bigint(tpStr.substr(10)));
		return false;
	}

	// Get operand recursively
	_node.subExpression().accept(*this);
	Expr::Ref subExpr = m_currentExpr;

	switch (_node.getOperator())
	{
	case Token::Add: m_currentExpr = subExpr; break; // Unary plus does not do anything
	case Token::Not: m_currentExpr = Expr::not_(subExpr); break;

	case Token::Sub:
	case Token::BitNot: {
		unsigned bits = ASTBoogieUtils::getBits(_node.annotation().type);
		bool isSigned = ASTBoogieUtils::isSigned(_node.annotation().type);
		auto exprResult = ASTBoogieUtils::encodeArithUnaryOp(m_context, &_node, _node.getOperator(), subExpr, bits, isSigned);
		m_currentExpr = exprResult.expr;
		if (m_context.overflow() && exprResult.cc)
			m_ocs.push_back(exprResult.cc);
		break;
	}

	// Inc and Dec shares most part of the code
	case Token::Inc:
	case Token::Dec:
		{
			unsigned bits = ASTBoogieUtils::getBits(_node.annotation().type);
			bool isSigned = ASTBoogieUtils::isSigned(_node.annotation().type);
			Expr::Ref lhs = subExpr;
			auto rhsResult =
					ASTBoogieUtils::encodeArithBinaryOp(m_context, &_node,
							_node.getOperator() == Token::Inc ? Token::Add : Token::Sub,
							lhs,
							m_context.intLit(1, bits),
							bits, isSigned);
			bg::Decl::Ref tempVar = bg::Decl::variable("inc#" + to_string(_node.id()),
					ASTBoogieUtils::toBoogieType(_node.subExpression().annotation().type, &_node, m_context));
			m_newDecls.push_back(tempVar);
			if (_node.isPrefixOperation()) // ++x (or --x)
			{
				// First do the assignment x := x + 1 (or x := x - 1)
				if (m_context.overflow() && rhsResult.cc)
					m_ocs.push_back(rhsResult.cc);
				createAssignment(_node.subExpression(), lhs, rhsResult.expr);
				// Then the assignment tmp := x
				addSideEffect(Stmt::assign(Expr::id(tempVar->getName()), lhs));
			}
			else // x++ (or x--)
			{
				// First do the assignment tmp := x
				addSideEffect(Stmt::assign(Expr::id(tempVar->getName()), subExpr));
				// Then the assignment x := x + 1 (or x := x - 1)
				if (m_context.overflow() && rhsResult.cc)
					m_ocs.push_back(rhsResult.cc);
				createAssignment(_node.subExpression(), lhs, rhsResult.expr);
			}
			// Result is the tmp variable (if the assignment is part of an expression)
			m_currentExpr = Expr::id(tempVar->getName());
		}
		break;
	default:
		m_context.reportError(&_node, string("Unsupported unary operator: ") + TokenTraits::toString(_node.getOperator()));
		m_currentExpr = Expr::id(ASTBoogieUtils::ERR_EXPR);
		break;
	}

	return false;
}

bool ASTBoogieExpressionConverter::visit(BinaryOperation const& _node)
{
	// Check if constant propagation could infer the result
	TypePointer tp = _node.annotation().type;
	if (auto tpRational = dynamic_cast<RationalNumberType const*>(tp))
	{
		m_currentExpr = Expr::lit(bg::bigint(tpRational->literalValue(nullptr)));
		return false;
	}

	// Get lhs recursively
	_node.leftExpression().accept(*this);
	Expr::Ref lhs = m_currentExpr;

	// Get rhs recursively
	_node.rightExpression().accept(*this);
	Expr::Ref rhs = m_currentExpr;

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
	case Token::And: m_currentExpr = Expr::and_(lhs, rhs); break;
	case Token::Or: m_currentExpr = Expr::or_(lhs, rhs); break;
	case Token::Equal: m_currentExpr = Expr::eq(lhs, rhs); break;
	case Token::NotEqual: m_currentExpr = Expr::neq(lhs, rhs); break;

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
		m_currentExpr = exprResult.expr;
		if (m_context.overflow() && exprResult.cc)
			m_ocs.push_back(exprResult.cc);
		break;
	}

	default:
		m_context.reportError(&_node, string("Unsupported binary operator ") + TokenTraits::toString(_node.getOperator()));
		m_currentExpr = Expr::id(ASTBoogieUtils::ERR_EXPR);
		break;
	}

	return false;
}

bool ASTBoogieExpressionConverter::visit(FunctionCall const& _node)
{
	// Check for conversions
	if (_node.annotation().kind == FunctionCallKind::TypeConversion)
	{
		functionCallConversion(_node);
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
	if (auto exprMa = dynamic_cast<MemberAccess const*>(&_node.expression()))
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
				TypePointer tp_uint256 = TypeProvider::integer(256, IntegerType::Modifier::Unsigned);
				m_currentMsgValue = ASTBoogieUtils::checkImplicitBvConversion(m_currentMsgValue,
						arg->annotation().type, tp_uint256, m_context);
			}

			// Continue with the rest of the AST
			exprMa->expression().accept(*this);
			return false;
		}
	}

	// Ignore gas setting, e.g., x.f.gas(y)(z) is just x.f(z)
	if (auto exprMa = dynamic_cast<MemberAccess const*>(&_node.expression()))
	{
		if (exprMa->memberName() == "gas")
		{
			m_context.reportWarning(exprMa, "Ignored call to gas() function.");
			exprMa->expression().accept(*this);
			return false;
		}
	}

	m_currentExpr = nullptr;
	m_currentAddress = m_context.boogieThis();
	m_currentMsgValue = nullptr;
	m_isGetter = false;
	m_isLibraryCall = false;
	// Process expression
	_node.expression().accept(*this);

	// 'm_currentExpr' should be an identifier, giving the name of the function
	string funcName;
	if (auto varExpr = dynamic_pointer_cast<bg::VarExpr const>(m_currentExpr))
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
	vector<Expr::Ref> allArgs;
	vector<Expr::Ref> regularArgs;
	// First, pass extra arguments
	if (m_isLibraryCall)
		allArgs.push_back(m_context.boogieThis()); // this
	else
		allArgs.push_back(m_currentAddress); // this
	allArgs.push_back(m_context.boogieThis()); // msg.sender
	Expr::Ref defaultMsgValue = m_context.intLit(0, 256);
	Expr::Ref msgValue = m_currentMsgValue ? m_currentMsgValue : defaultMsgValue;
	allArgs.push_back(msgValue); // msg.value
	if (m_isLibraryCall && !m_isLibraryCallStatic)
		allArgs.push_back(m_currentAddress); // Non-static library calls require extra argument

	for (unsigned i = 0; i < _node.arguments().size(); ++i)
	{
		auto arg = _node.arguments()[i];
		arg->accept(*this);
		// In bit-precise mode, we need the definition of the function to see the types of the parameters
		// (for implicit conversions)
		if (m_context.isBvEncoding())
		{
			// Try to get the definition
			FunctionDefinition const* calledFunc = nullptr;
			if (auto exprId = dynamic_cast<Identifier const*>(&_node.expression()))
			{
				if (auto funcDef = dynamic_cast<FunctionDefinition const *>(exprId->annotation().referencedDeclaration))
					calledFunc = funcDef;
			}
			if (auto exprMa = dynamic_cast<MemberAccess const*>(&_node.expression()))
			{
				if (auto funcDef = dynamic_cast<FunctionDefinition const *>(exprMa->annotation().referencedDeclaration))
					calledFunc = funcDef;
			}
			// TODO: the above does not work for complex expressions like 'x.f.value(1)()'

			if (calledFunc)
				m_currentExpr = ASTBoogieUtils::checkImplicitBvConversion(m_currentExpr, arg->annotation().type, calledFunc->parameters()[i]->annotation().type, m_context);
		}

		// Do not add argument for call
		if (funcName != ASTBoogieUtils::BOOGIE_CALL)
		{
			allArgs.push_back(m_currentExpr);
			regularArgs.push_back(m_currentExpr);
			// Array length
			if (arg->annotation().type && arg->annotation().type->category() == Type::Category::Array)
			{
				allArgs.push_back(getArrayLength(m_currentExpr, *arg));
				regularArgs.push_back(getArrayLength(m_currentExpr, *arg));
			}
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
		addSideEffect(Stmt::assert_(regularArgs[0], ASTBoogieUtils::createAttrs(_node.location(), "Assertion might not hold.", *m_context.currentScanner())));
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
		addSideEffect(Stmt::assume(regularArgs[0]));
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

		addSideEffect(Stmt::assume(Expr::lit(false)));
		return false;
	}

	// Sum function
	if (boost::algorithm::starts_with(funcName, ASTBoogieUtils::VERIFIER_SUM))
	{
		functionCallSum(_node);
		return false;
	}

	// Old function
	if (boost::algorithm::starts_with(funcName, ASTBoogieUtils::VERIFIER_OLD))
	{
		functionCallOld(_node, regularArgs);
		return false;
	}

	// Struct initialization
	if (auto exprId = dynamic_cast<Identifier const*>(&_node.expression()))
	{
		if (auto structDef = dynamic_cast<StructDefinition const *>(exprId->annotation().referencedDeclaration))
		{
			functionCallNewStruct(_node, structDef, regularArgs);
			return false;
		}
	}

	// If msg.value was set, we should reduce our own balance (and the called function will increase its own)
	if (msgValue != defaultMsgValue) functionCallReduceBalance(msgValue);

	// External calls require the invariants to hold
	if (funcName == ASTBoogieUtils::BOOGIE_CALL)
	{
		for (auto invar : m_context.currentContractInvars())
		{
			for (auto tcc : invar.tccs)
			{
				addSideEffect(Stmt::assert_(tcc,
						ASTBoogieUtils::createAttrs(_node.location(), "Variables for invariant '" + invar.exprStr + "' might be out of range before external call.", *m_context.currentScanner())));
			}
			addSideEffect(Stmt::assert_(invar.expr,
					ASTBoogieUtils::createAttrs(_node.location(), "Invariant '" + invar.exprStr + "' might not hold before external call.", *m_context.currentScanner())));
		}
	}

	auto returnType = _node.annotation().type;
	auto returnTupleType = dynamic_cast<TupleType const*>(returnType);

	// Create fresh variables to store the result of the function call
	std::vector<std::string> returnVarNames;
	std::vector<Expr::Ref> returnVars;
	if (returnTupleType)
	{
		auto const& returnTypes = returnTupleType->components();
		solAssert(returnTypes.size() != 1, "");
		for (size_t i = 0; i < returnTypes.size(); ++ i)
		{
			auto varName = funcName + "#" + to_string(i) + "#" + to_string(_node.id());
			auto varType = ASTBoogieUtils::toBoogieType(returnTypes[i], &_node, m_context);
			auto varDecl = bg::Decl::variable(varName, varType);
			m_newDecls.push_back(varDecl);
			returnVarNames.push_back(varName);
			returnVars.push_back(Expr::id(varName));
		}
	}
	else
	{
		// New expressions already create the fresh variable
		if (!dynamic_cast<NewExpression const*>(&_node.expression()))
		{
			auto varName = funcName + "#" + to_string(_node.id());
			auto varType = ASTBoogieUtils::toBoogieType(returnType, &_node, m_context);
			auto varDecl = bg::Decl::variable(varName, varType);
			m_newDecls.push_back(varDecl);
			returnVarNames.push_back(varName);
			returnVars.push_back(Expr::id(varName));
		}
	}

	if (m_isGetter)
	{
		// Getters are replaced with map access (instead of function call)
		m_currentExpr = returnVars[0];
		addSideEffect(Stmt::assign(returnVars[0], Expr::sel(Expr::id(funcName), m_currentAddress)));
	}
	else
	{
		// Assign call to the fresh variable
		addSideEffects({
			Stmt::annot(ASTBoogieUtils::createAttrs(_node.location(), "", *m_context.currentScanner())),
			Stmt::call(funcName, allArgs, returnVarNames)
		});

		// Result is the none, single variable, or a tuple of variables
		if (returnVars.size() == 0)
		{
			// For new expressions there is no return value, but the address should be used
			if (dynamic_cast<NewExpression const*>(&_node.expression())) m_currentExpr = m_currentAddress;
			else m_currentExpr = nullptr;
		}
		else if (returnVars.size() == 1)
		{
			m_currentExpr = returnVars[0];
		}
		else
		{
			m_currentExpr = Expr::tuple(returnVars);
		}

		// Assume invariants after external call
		if (funcName == ASTBoogieUtils::BOOGIE_CALL)
		{
			for (auto invar : m_context.currentContractInvars())
			{
				for (auto tcc : invar.tccs)
					addSideEffect(Stmt::assume(tcc));
				addSideEffect(Stmt::assume(invar.expr));
			}
		}

		// The call function is special as it indicates failure in a return value and in this case
		// we must undo reducing our balance
		if (funcName == ASTBoogieUtils::BOOGIE_CALL && msgValue != defaultMsgValue)
		{
			functionCallRevertBalance(msgValue);
		}
	}

	return false;
}

void ASTBoogieExpressionConverter::functionCallConversion(FunctionCall const& _node)
{
	solAssert(_node.arguments().size() == 1, "Type conversion should have exactly one argument");
	auto arg = _node.arguments()[0];
	// Converting to address
	bool toAddress = false;
	if (auto expr = dynamic_cast<ElementaryTypeNameExpression const*>(&_node.expression()))
		if (expr->typeName().token() == Token::Address)
			toAddress = true;

	// Converting to other kind of contract
	if (auto expr = dynamic_cast<Identifier const*>(&_node.expression()))
		if (dynamic_cast<ContractDefinition const *>(expr->annotation().referencedDeclaration))
			toAddress = true;

	if (toAddress)
	{
		arg->accept(*this);
		return;
	}

	bool converted = false;
	TypeDeclRef targetType = ASTBoogieUtils::toBoogieType(_node.annotation().type, &_node, m_context);
	TypeDeclRef sourceType = ASTBoogieUtils::toBoogieType(arg->annotation().type, arg.get(), m_context);
	// Nothing to do when the two types are mapped to same type in Boogie,
	// e.g., conversion from uint256 to int256 if both are mapped to int
	// TODO: check the type instance instead of name as soon as it is a singleton
	if (targetType->getName() == sourceType->getName() || (targetType->getName() == "int" && sourceType->getName() == "int_const"))
	{
		arg->accept(*this);
		converted = true;
	}

	if (m_context.isBvEncoding() && ASTBoogieUtils::isBitPreciseType(_node.annotation().type))
	{
		arg->accept(*this);
		m_currentExpr = ASTBoogieUtils::checkExplicitBvConversion(m_currentExpr, arg->annotation().type, _node.annotation().type, m_context);
		converted = true;
	}

	if (converted)
	{
		// Range assertion for enums
		if (auto exprId = dynamic_cast<Identifier const*>(&_node.expression()))
		{
			if (auto enumDef = dynamic_cast<EnumDefinition const*>(exprId->annotation().referencedDeclaration))
			{
				m_newStatements.push_back(bg::Stmt::assert_(
						bg::Expr::and_(
								ASTBoogieUtils::encodeArithBinaryOp(m_context, &_node, langutil::Token::LessThanOrEqual,
										m_context.intLit(0, 256), m_currentExpr, 256, false).expr,
								ASTBoogieUtils::encodeArithBinaryOp(m_context, &_node, langutil::Token::LessThan,
										m_currentExpr, m_context.intLit(enumDef->members().size(), 256), 256, false).expr),
						ASTBoogieUtils::createAttrs(_node.location(), "Conversion to enum might be out of range", *m_context.currentScanner())));
			}
		}
	}
	else
	{
		m_context.reportError(&_node, "Unsupported type conversion");
		m_currentExpr = Expr::id(ASTBoogieUtils::ERR_EXPR);
	}
}

Decl::Ref ASTBoogieExpressionConverter::newStruct(StructDefinition const* structDef, string id)
{
	// Address of the new struct
	// TODO: make sure that it is a new address
	string varName = "new_struct_" + structDef->name() + "#" + id;
	TypeDeclRef varType = ASTBoogieUtils::getStructAddressType(structDef, DataLocation::Memory);
	auto varDecl = bg::Decl::variable(varName, varType);
	m_newDecls.push_back(varDecl);
	return varDecl;
}

void ASTBoogieExpressionConverter::functionCallNewStruct(FunctionCall const& _node,
		StructDefinition const* structDef, vector<Expr::Ref> const& args)
{
	auto varDecl = newStruct(structDef, toString(_node.id()));
	// Initialize each member
	for (size_t i = 0; i < structDef->members().size(); ++i)
	{
		auto member = bg::Expr::id(m_context.mapStructMemberName(*structDef->members()[i], DataLocation::Memory));
		auto init = bg::Expr::upd(member, bg::Expr::id(varDecl->getName()), args[i]);
		m_newStatements.push_back(bg::Stmt::assign(member, init));
	}
	// Return the address
	m_currentExpr = bg::Expr::id(varDecl->getName());
}

void ASTBoogieExpressionConverter::functionCallReduceBalance(Expr::Ref msgValue)
{
	TypePointer tp_uint256 = TypeProvider::integer(256, IntegerType::Modifier::Unsigned);
	// assert(balance[this] >= msg.value)
	auto selExpr = Expr::sel(m_context.boogieBalance(), m_context.boogieThis());
	auto geqResult = ASTBoogieUtils::encodeArithBinaryOp(m_context, nullptr,
			langutil::Token::GreaterThanOrEqual, selExpr, msgValue, 256, false);
	addSideEffect(Stmt::comment("Implicit assumption that we have enough ether"));
	addSideEffect(Stmt::assume(geqResult.expr));
	// balance[this] -= msg.value
	Expr::Ref this_balance = Expr::sel(m_context.boogieBalance(),
			m_context.boogieThis());
	if (m_context.encoding() == BoogieContext::Encoding::MOD)
	{
		addSideEffect(Stmt::assume(ASTBoogieUtils::getTCCforExpr(this_balance, tp_uint256)));
		addSideEffect(Stmt::assume(ASTBoogieUtils::getTCCforExpr(msgValue, tp_uint256)));
	}
	auto subResult = ASTBoogieUtils::encodeArithBinaryOp(m_context, nullptr, Token::Sub,
			this_balance, msgValue, 256, false);
	if (m_context.overflow())
	{
		addSideEffect(Stmt::comment("Implicit assumption that balances cannot overflow"));
		addSideEffect(Stmt::assume(subResult.cc));
	}
	addSideEffect(
			Stmt::assign(m_context.boogieBalance(),
					Expr::upd(m_context.boogieBalance(),
							m_context.boogieThis(), subResult.expr)));
}

void ASTBoogieExpressionConverter::functionCallRevertBalance(Expr::Ref msgValue)
{
	TypePointer tp_uint256 = TypeProvider::integer(256, IntegerType::Modifier::Unsigned);
	bg::Block::Ref revert = bg::Block::block();
	// balance[this] += msg.value
	Expr::Ref this_balance = Expr::sel(m_context.boogieBalance(), m_context.boogieThis());
	if (m_context.encoding() == BoogieContext::Encoding::MOD)
	{
		revert->addStmts( { Stmt::assume(ASTBoogieUtils::getTCCforExpr(this_balance, tp_uint256)),
			Stmt::assume(ASTBoogieUtils::getTCCforExpr(msgValue, tp_uint256)) });
	}
	auto addResult = ASTBoogieUtils::encodeArithBinaryOp(m_context, nullptr, Token::Add,
			this_balance, msgValue, 256, false);
	if (m_context.overflow())
	{
		revert->addStmts( { Stmt::comment("Implicit assumption that balances cannot overflow"),
			Stmt::assume(addResult.cc) });
	}
	revert->addStmt(
			Stmt::assign(m_context.boogieBalance(),
					Expr::upd(m_context.boogieBalance(),
							m_context.boogieThis(), addResult.expr)));
	// Final statement for balance update in case of failure. Return value of call
	// is always a tuple (ok, data).
	auto okDataTuple = dynamic_pointer_cast<TupleExpr const>(m_currentExpr);
	solAssert(okDataTuple, "");
	addSideEffect(Stmt::ifelse(Expr::not_(okDataTuple->elements()[0]), revert));
}

void ASTBoogieExpressionConverter::functionCallSum(FunctionCall const& _node)
{
	TypePointer tp = _node.annotation().type;
	if (auto id = dynamic_pointer_cast<Identifier const>(_node.arguments()[0]))
	{
		auto sumDecl = id->annotation().referencedDeclaration;
		m_context.currentSumDecls()[sumDecl] = tp;

		auto declCategory = id->annotation().type->category();
		if (declCategory != Type::Category::Mapping && declCategory != Type::Category::Array)
			m_context.reportError(&_node, "Argument of sum must be an array or a mapping");
	}
	else
		m_context.reportError(&_node, "Argument of sum must be an identifier");

	m_currentExpr = getSumShadowVar(_node.arguments()[0].get());
	addTCC(m_currentExpr, tp);
}

void ASTBoogieExpressionConverter::functionCallOld(FunctionCall const& _node, vector<Expr::Ref> const& args)
{
	solAssert(args.size() == 1, "Verifier old function must have exactly one argument");
	m_currentExpr = Expr::old(args[0]);
	addTCC(m_currentExpr, _node.annotation().type);
}

bool ASTBoogieExpressionConverter::visit(NewExpression const& _node)
{

	if (auto userDefType = dynamic_cast<UserDefinedTypeName const*>(&_node.typeName()))
	{
		if (auto contract = dynamic_cast<ContractDefinition const*>(userDefType->annotation().referencedDeclaration))
		{
			// TODO: Make sure that it is a fresh address
			m_currentExpr = Expr::id(ASTBoogieUtils::getConstructorName(contract));
			auto varDecl = bg::Decl::variable("new#" + toString(_node.id()), m_context.addressType());
			m_newDecls.push_back(varDecl);
			m_currentAddress = bg::Expr::id(varDecl->getName());
			return false;
		}
	}

	m_context.reportError(&_node, "Unsupported new expression");
	m_currentExpr = Expr::id(ASTBoogieUtils::ERR_EXPR);
	return false;
}

bool ASTBoogieExpressionConverter::visit(MemberAccess const& _node)
{
	// Normally, the expression of the MemberAccess will give the address and
	// the memberName will give the name. For example, x.f() will have address
	// 'x' and name 'f'.

	// Get expression recursively
	_node.expression().accept(*this);
	Expr::Ref expr = m_currentExpr;
	// The current expression gives the address on which something is done
	m_currentAddress = m_currentExpr;
	// If we are accessing something on 'super', the current address should be 'this'
	// and not 'super', because that does not exist
	if (auto id = dynamic_cast<Identifier const*>(&_node.expression()))
		if (dynamic_cast<MagicVariableDeclaration const*>(id->annotation().referencedDeclaration) &&
				id->annotation().referencedDeclaration->name() == ASTBoogieUtils::SOLIDITY_SUPER)
			m_currentAddress = m_context.boogieThis();

	// Type of the expression
	TypePointer type = _node.expression().annotation().type;
	Type::Category typeCategory = type->category();

	// Handle special members/functions
	TypePointer tp_uint256 = TypeProvider::integer(256, IntegerType::Modifier::Unsigned);

	// address.balance / this.balance
	bool isAddress = typeCategory == Type::Category::Address;
	if (_node.memberName() == ASTBoogieUtils::SOLIDITY_BALANCE)
	{
		if (isAddress)
		{
			m_currentExpr = Expr::sel(m_context.boogieBalance(), expr);
			addTCC(m_currentExpr, tp_uint256);
			return false;
		}
		if (auto exprId = dynamic_cast<Identifier const*>(&_node.expression()))
		{
			if (exprId->name() == ASTBoogieUtils::SOLIDITY_THIS)
			{
				m_currentExpr = Expr::sel(m_context.boogieBalance(), m_context.boogieThis());
				addTCC(m_currentExpr, tp_uint256);
				return false;
			}
		}
	}
	// address.transfer()
	if (isAddress && _node.memberName() == ASTBoogieUtils::SOLIDITY_TRANSFER)
	{
		m_context.includeTransferFunction();
		m_currentExpr = Expr::id(ASTBoogieUtils::BOOGIE_TRANSFER);
		return false;
	}
	// address.send()
	if (isAddress && _node.memberName() == ASTBoogieUtils::SOLIDITY_SEND)
	{
		m_context.includeSendFunction();
		m_currentExpr = Expr::id(ASTBoogieUtils::BOOGIE_SEND);
		return false;
	}
	// address.call()
	if (isAddress && _node.memberName() == ASTBoogieUtils::SOLIDITY_CALL)
	{
		m_context.includeCallFunction();
		m_currentExpr = Expr::id(ASTBoogieUtils::BOOGIE_CALL);
		return false;
	}
	// msg.sender
	auto magicType = dynamic_cast<MagicType const*>(type);
	bool isMessage = magicType != nullptr && magicType->kind() == MagicType::Kind::Message;
	if (isMessage && _node.memberName() == ASTBoogieUtils::SOLIDITY_SENDER)
	{
		m_currentExpr = m_context.boogieMsgSender();
		return false;
	}
	// msg.value
	if (isMessage && _node.memberName() == ASTBoogieUtils::SOLIDITY_VALUE)
	{
		m_currentExpr = m_context.boogieMsgValue();
		TypePointer tp_uint256 = TypeProvider::integer(256, IntegerType::Modifier::Unsigned);
		addTCC(m_currentExpr, tp_uint256);
		return false;
	}
	// block
	bool isBlock = magicType != nullptr && magicType->kind() == MagicType::Kind::Block;
	// block.number
	if (isBlock && _node.memberName() == ASTBoogieUtils::SOLIDITY_NUMBER)
	{
		m_currentExpr = Expr::id(ASTBoogieUtils::BOOGIE_BLOCKNO);
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
		auto fbType = dynamic_cast<FixedBytesType const*>(_node.expression().annotation().type);
		m_currentExpr = Expr::lit(fbType->numBytes());
		return false;
	}

	// Enums
	if (_node.annotation().type->category() == Type::Category::Enum)
	{
		if (auto exprId = dynamic_cast<Identifier const*>(&_node.expression()))
		{
			if (auto enumDef = dynamic_cast<EnumDefinition const*>(exprId->annotation().referencedDeclaration))
			{
				// TODO: better way to get index?
				for (size_t i = 0; i < enumDef->members().size(); ++i)
				{
					if (enumDef->members()[i]->name() == _node.memberName())
					{
						m_currentExpr = m_context.intLit(i, 256);
						return false;
					}
				}
				solAssert(false, "Enum member not found");
			}
		}
		solAssert(false, "Enum definition not found");
	}

	// Non-special member access: 'referencedDeclaration' should point to the
	// declaration corresponding to 'memberName'
	if (_node.annotation().referencedDeclaration == nullptr)
	{
		m_context.reportError(&_node, "Member without corresponding declaration (probably an unsupported special member)");
		m_currentExpr = Expr::id(ASTBoogieUtils::ERR_EXPR);
		return false;
	}
	m_currentExpr = Expr::id(m_context.mapDeclName(*_node.annotation().referencedDeclaration));
	// Check for getter
	m_isGetter =  dynamic_cast<VariableDeclaration const*>(_node.annotation().referencedDeclaration);
	// Check for library call
	m_isLibraryCall = false;
	if (auto fDef = dynamic_cast<FunctionDefinition const*>(_node.annotation().referencedDeclaration))
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
			return false;
		}
	}

	// Member access on structures: create selector expression
	if (typeCategory == Type::Category::Struct)
	{
		auto structType = dynamic_cast<StructType const*>(type);
		m_currentExpr = Expr::id(m_context.mapStructMemberName(*_node.annotation().referencedDeclaration, structType->location()));
		m_currentExpr = Expr::sel(m_currentExpr, m_currentAddress);
	}

	return false;
}

bool ASTBoogieExpressionConverter::visit(IndexAccess const& _node)
{
	Expression const& base = _node.baseExpression();
	base.accept(*this);
	Expr::Ref baseExpr = m_currentExpr;

	Expression const* index = _node.indexExpression();
	index->accept(*this); // TODO: can this be a nullptr?
	Expr::Ref indexExpr = m_currentExpr;

	TypePointer baseType = base.annotation().type;
	TypePointer indexType = index->annotation().type;

	// Fixed size byte arrays
	if (baseType->category() == Type::Category::FixedBytes)
	{
		auto fbType = dynamic_cast<FixedBytesType const*>(baseType);
		unsigned fbSize = fbType->numBytes();

		// Check bounds (typechecked for unsigned, so >= 0)
		addSideEffect(Stmt::assume(Expr::gte(indexExpr, Expr::lit((unsigned)0))));
		addSideEffect(Stmt::assert_(Expr::lt(indexExpr, Expr::lit(fbSize)),
					ASTBoogieUtils::createAttrs(_node.location(), "Index may be out of bounds", *m_context.currentScanner())));

		// Do a case split on which slice to use
		for (unsigned i = 0; i < fbSize; ++ i)
		{
			Expr::Ref slice = m_context.intSlice(baseExpr, fbSize*8, (i+1)*8 - 1, i*8);
			if (i == 0)
				m_currentExpr = slice;
			else
			{
				m_currentExpr = Expr::if_then_else(
						Expr::eq(indexExpr, Expr::lit(i)),
						slice, m_currentExpr
				);
			}
		}
		return false;
	}

	if (baseType->category() == Type::Category::Array && m_context.isBvEncoding())
	{
		// For arrays, cast to uint
		TypePointer tp_uint256 = TypeProvider::integer(256, IntegerType::Modifier::Unsigned);
		indexExpr = ASTBoogieUtils::checkImplicitBvConversion(indexExpr, indexType, tp_uint256, m_context);
	}
	// Index access is simply converted to a select in Boogie, which is fine
	// as long as it is not an LHS of an assignment (e.g., x[i] = v), but
	// that case is handled when converting assignments
	m_currentExpr = Expr::sel(baseExpr, indexExpr);
	addTCC(m_currentExpr, _node.annotation().type);

	return false;
}

bool ASTBoogieExpressionConverter::visit(Identifier const& _node)
{
	if (_node.name() == ASTBoogieUtils::VERIFIER_SUM)
	{
		m_currentExpr = Expr::id(ASTBoogieUtils::VERIFIER_SUM);
		return false;
	}

	if (!_node.annotation().referencedDeclaration)
	{
		m_context.reportError(&_node, "Identifier '" + _node.name() + "' has no matching declaration");
		m_currentExpr = Expr::id(ASTBoogieUtils::ERR_EXPR);
		return false;
	}

	// Inline constants
	if (auto varDecl = dynamic_cast<VariableDeclaration const*>(_node.annotation().referencedDeclaration))
	{
		if (varDecl->isConstant())
		{
			varDecl->value()->accept(*this);
			return false;
		}
	}

	string declName = m_context.mapDeclName(*(_node.annotation().referencedDeclaration));

	// State variables must be referenced by accessing the map
	if (ASTBoogieUtils::isStateVar(_node.annotation().referencedDeclaration))
		m_currentExpr = Expr::sel(Expr::id(declName), m_context.boogieThis());
	// Other identifiers can be referenced by their name
	else
		m_currentExpr = Expr::id(declName);

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

	switch (typeCategory)
	{
	case Type::Category::RationalNumber:
	{
		auto rationalType = dynamic_cast<RationalNumberType const*>(type);
		if (rationalType != nullptr)
		{
			m_currentExpr = boogie::Expr::lit(rationalType->literalValue(nullptr));
			return false;
		}
		break;
	}
	case Type::Category::Bool:
		m_currentExpr = Expr::lit(_node.value() == "true");
		return false;
	case Type::Category::Address:
	{
		string name = "address_" + _node.value();
		m_newConstants.push_back(bg::Decl::constant(name, m_context.addressType(), true));
		m_currentExpr = Expr::id(name);
		return false;
	}
	case Type::Category::StringLiteral:
	{
		string name = "literal_string#" + to_string(_node.id());
		m_newConstants.push_back(bg::Decl::constant(name, m_context.stringType(), true));
		m_currentExpr = Expr::id(name);
		return false;
	}
	default:
	{
		// Report unsupported
		string tpStr = type->toString();
		m_context.reportError(&_node, "Unsupported literal for type " + tpStr.substr(0, tpStr.find(' ')));
		m_currentExpr = Expr::id(ASTBoogieUtils::ERR_EXPR);
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
