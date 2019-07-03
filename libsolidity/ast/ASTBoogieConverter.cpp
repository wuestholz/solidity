#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <libsolidity/analysis/NameAndTypeResolver.h>
#include <libsolidity/analysis/TypeChecker.h>
#include <libsolidity/ast/ASTBoogieConverter.h>
#include <libsolidity/ast/ASTBoogieExpressionConverter.h>
#include <libsolidity/ast/ASTBoogieUtils.h>
#include <libsolidity/ast/TypeProvider.h>
#include <libsolidity/parsing/Parser.h>
#include <liblangutil/SourceReferenceFormatter.h>
#include <liblangutil/ErrorReporter.h>

#include <map>

using namespace std;
using namespace dev;
using namespace dev::solidity;
using namespace langutil;

namespace dev
{
namespace solidity
{

string const ASTBoogieConverter::DOCTAG_CONTRACT_INVAR = "invariant";
string const ASTBoogieConverter::DOCTAG_CONTRACT_INVARS_INCLUDE = "{contractInvariants}";
string const ASTBoogieConverter::DOCTAG_LOOP_INVAR = "invariant";
string const ASTBoogieConverter::DOCTAG_PRECOND = "precondition";
string const ASTBoogieConverter::DOCTAG_POSTCOND = "postcondition";
string const ASTBoogieConverter::DOCTAG_MODIFIES = "modifies";
string const ASTBoogieConverter::DOCTAG_MODIFIES_ALL = DOCTAG_MODIFIES + " *";
string const ASTBoogieConverter::DOCTAG_MODIFIES_COND = " if ";

boogie::Expr::Ref ASTBoogieConverter::convertExpression(Expression const& _node)
{
	ASTBoogieExpressionConverter::Result result = ASTBoogieExpressionConverter(m_context, m_currentContract).convert(_node);

	m_localDecls.insert(end(m_localDecls), begin(result.newDecls), end(result.newDecls));
	for (auto tcc: result.tccs)
		m_currentBlocks.top()->addStmt(boogie::Stmt::assume(tcc));
	for (auto s: result.newStatements)
		m_currentBlocks.top()->addStmt(s);
	for (auto oc: result.ocs)
		m_currentBlocks.top()->addStmt(boogie::Stmt::assign(
			boogie::Expr::id(ASTBoogieUtils::VERIFIER_OVERFLOW),
			boogie::Expr::or_(boogie::Expr::id(ASTBoogieUtils::VERIFIER_OVERFLOW), boogie::Expr::not_(oc))));
	for (auto c: result.newConstants)
		m_context.addConstant(c);

	return result.expr;
}

boogie::Expr::Ref ASTBoogieConverter::defaultValue(TypePointer type, BoogieContext& context)
{
	switch (type->category())
	{
	case Type::Category::Integer:
		// 0
		return context.intLit(0, ASTBoogieUtils::getBits(type));
	case Type::Category::Bool:
		// False
		return boogie::Expr::lit(false);
	case Type::Category::Struct:
	{
		// default for all members
		StructType const* structType = dynamic_cast<StructType const*>(type);
		MemberList const& members = structType->members(0); // No need for scope, just regular struct members
		// Get the default value for the members
		std::vector<boogie::Expr::Ref> memberDefaultValues;
		for (auto& member: members)
		{
			boogie::Expr::Ref memberDefaultValue = defaultValue(member.type, context);
			if (memberDefaultValue == nullptr)
				return nullptr;
			memberDefaultValues.push_back(memberDefaultValue);
		}
		// Now construct the struct value
		StructDefinition const& structDefinition = structType->structDefinition();
		boogie::FuncDeclRef decl = context.getStructConstructor(&structDefinition);
		return boogie::Expr::fn(decl->getName(), memberDefaultValues);
	}
	default:
		// For unhandled, just return null
		break;
	}

	return nullptr;
}

void ASTBoogieConverter::getVariablesOfType(TypePointer _type, ASTNode const& _scope, std::vector<boogie::Expr::Ref>& output)
{
	std::string target = _type->toString();
	DeclarationContainer const* decl_container = m_context.scopes()[&_scope].get();
	for (; decl_container != nullptr; decl_container = decl_container->enclosingContainer())
	{
		for (auto const& decl_pair: decl_container->declarations())
		{
			auto const& decl_vector = decl_pair.second;
			for (auto const& decl: decl_vector)
			{
				if (decl->type()->toString() == target)
				{
					if (ASTBoogieUtils::isStateVar(decl))
						output.push_back(boogie::Expr::arrsel(boogie::Expr::id(m_context.mapDeclName(*decl)), m_context.boogieThis()->getRefTo()));
					else
						output.push_back(boogie::Expr::id(m_context.mapDeclName(*decl)));
				}
				else
				{
					// Structs: go through fields: TODO: memory structs, recursion
					if (decl->type()->category() == Type::Category::Struct)
					{
						auto s = dynamic_cast<StructType const*>(decl->type());
						for (auto const& s_member: s->members(nullptr))
							if (s_member.declaration->type()->toString() == target)
							{
								if (ASTBoogieUtils::isStateVar(decl))
									boogie::Expr::dtsel(boogie::Expr::arrsel(boogie::Expr::id(m_context.mapDeclName(*decl)), m_context.boogieThis()->getRefTo()),
											m_context.mapDeclName(*s_member.declaration),
											m_context.getStructConstructor(&s->structDefinition()),
											dynamic_pointer_cast<boogie::DataTypeDecl>(m_context.getStructType(&s->structDefinition(), DataLocation::Storage)));
							}
					}
					// Magic
					if (decl->type()->category() == Type::Category::Magic)
					{
						auto m = dynamic_cast<MagicType const*>(decl->type());
						for (auto const& m_member: m->members(nullptr))
							if (m_member.type->toString() == target)
								// Only sender for now. TODO: Better handling of magic variables
								// and names
								if (m_member.name == ASTBoogieUtils::SOLIDITY_SENDER)
									output.push_back(m_context.boogieMsgSender()->getRefTo());
					}
				}
			}
		}
	}
}

bool ASTBoogieConverter::defaultValueAssignment(VariableDeclaration const& _decl, ASTNode const& _scope, std::vector<boogie::Stmt::Ref>& output)
{
	bool ok = false;

	std::string id = m_context.mapDeclName(_decl);
	TypePointer type = _decl.type();

	// Default value for the given type
	boogie::Expr::Ref value = defaultValue(type, m_context);

	// If there just assign
	if (value)
	{
		boogie::Stmt::Ref valueAssign = boogie::Stmt::assign(boogie::Expr::id(id), boogie::Expr::arrupd(
				boogie::Expr::id(id), m_context.boogieThis()->getRefTo(), value));
		output.push_back(valueAssign);
		ok = true;
	}
	else
	{
		// Otherwise, it's probably a complex type
		switch (type->category())
		{
		case Type::Category::Mapping:
		{
			// Type of the index and element
			TypePointer key_type = dynamic_cast<MappingType const&>(*type).keyType();
			TypePointer element_type = dynamic_cast<MappingType const&>(*type).valueType();
			// Default value for elements
			value = defaultValue(element_type, m_context);
			if (value)
			{
				// Get all ids to initialize
				std::vector<boogie::Expr::Ref> index_ids;
				getVariablesOfType(key_type, _scope, index_ids);
				// Initialize all instantiations to default value
				for (auto index_id: index_ids)
				{
					// a[this][i] = v
					// a = update(a, this
					//        update(sel(a, this), i, v)
					//     )
					auto map_id = boogie::Expr::id(id);
					auto this_i = m_context.boogieThis()->getRefTo();
					auto valueAssign = boogie::Stmt::assign(map_id,
							boogie::Expr::arrupd(map_id, this_i,
									boogie::Expr::arrupd(boogie::Expr::arrsel(map_id, this_i), index_id, value)));
					output.push_back(valueAssign);
				}
				// Initialize the sum, if there, to default value
				if (m_context.currentSumDecls()[&_decl])
				{
					boogie::Expr::Ref sum = boogie::Expr::id(id + ASTBoogieUtils::BOOGIE_SUM);
					boogie::Stmt::Ref sum_default = boogie::Stmt::assign(sum,
							boogie::Expr::arrupd(sum, m_context.boogieThis()->getRefTo(), value));
					output.push_back(sum_default);
				}
				ok = true;
			}
			break;
		}
		default:
			// Return null
			break;
		}
	}

	return ok;
}

void ASTBoogieConverter::createImplicitConstructor(ContractDefinition const& _node)
{
	m_context.addGlobalComment("\nDefault constructor");

	m_localDecls.clear();

	// Include preamble
	m_currentBlocks.push(boogie::Block::block());
	constructorPreamble(_node);
	boogie::Block::Ref block = m_currentBlocks.top();
	m_currentBlocks.pop();
	solAssert(m_currentBlocks.empty(), "Non-empty stack of blocks at the end of function.");

	string funcName = ASTBoogieUtils::getConstructorName(&_node);

	// Input parameters
	std::vector<boogie::Binding> params {
		{m_context.boogieThis()->getRefTo(), m_context.boogieThis()->getType() }, // this
		{m_context.boogieMsgSender()->getRefTo(), m_context.boogieMsgSender()->getType() }, // msg.sender
		{m_context.boogieMsgValue()->getRefTo(), m_context.boogieMsgValue()->getType() } // msg.value
	};

	// Create the procedure
	auto procDecl = boogie::Decl::procedure(funcName, params, {}, m_localDecls, {block});
	for (auto invar: m_context.currentContractInvars())
	{
		auto attrs = ASTBoogieUtils::createAttrs(_node.location(), "State variable initializers might violate invariant '" + invar.exprStr + "'.", *m_context.currentScanner());
		procDecl->getEnsures().push_back(boogie::Specification::spec(invar.expr, attrs));
	}
	auto attrs = ASTBoogieUtils::createAttrs(_node.location(),  _node.name() + "::[implicit_constructor]", *m_context.currentScanner());
	procDecl->addAttrs(attrs);
	m_context.addDecl(procDecl);
}

void ASTBoogieConverter::constructorPreamble(ASTNode const& _scope)
{
	TypePointer tp_uint256 = TypeProvider::integer(256, IntegerType::Modifier::Unsigned);

	// this.balance = 0
	m_currentBlocks.top()->addStmt(boogie::Stmt::assign(
			m_context.boogieBalance()->getRefTo(),
			boogie::Expr::arrupd(
					m_context.boogieBalance()->getRefTo(),
					m_context.boogieThis()->getRefTo(),
					defaultValue(tp_uint256, m_context))));
	// Assign state vars that have initializers
	for (auto i: m_stateVarInitializers)
		m_currentBlocks.top()->addStmt(i);
	m_localDecls.insert(end(m_localDecls), begin(m_stateVarInitTmpDecls), end(m_stateVarInitTmpDecls));
	// Assign uninitialized state variables to default values
	for (auto declNode: m_stateVarsToInitialize)
	{
		std::vector<boogie::Stmt::Ref> stmts;
		bool ok = defaultValueAssignment(*declNode, _scope, stmts);
		if (!ok)
			m_context.reportWarning(declNode, "Boogie: Unhandled default value, constructor verification might fail.");
		for (auto stmt: stmts)
			m_currentBlocks.top()->addStmt(stmt);
	}
}

bool ASTBoogieConverter::parseExpr(string exprStr, ASTNode const& _node, ASTNode const* _scope, BoogieContext::DocTagExpr& result)
{
	// We temporarily replace the error reporter in the context, because the locations
	// are pointing to positions in the docstring
	ErrorList errorList;
	ErrorReporter errorReporter(errorList);
	TypeChecker typeChecker(m_context.evmVersion(), errorReporter, m_currentContract);

	ErrorReporter* originalErrReporter = m_context.errorReporter();
	m_context.errorReporter() = &errorReporter;

	try
	{
		// Parse
		CharStream exprStream(exprStr, "DocString");
		ASTPointer<Expression> expr = Parser(*m_context.errorReporter(), m_context.evmVersion())
			.parseExpression(std::make_shared<Scanner>(exprStream));
		if (!expr)
			throw langutil::FatalError();

		// Resolve references, using the given scope
		m_context.scopes()[expr.get()] = m_context.scopes()[_scope];
		NameAndTypeResolver resolver(*m_context.globalContext(), m_context.scopes(), *m_context.errorReporter());
		if (resolver.resolveNamesAndTypes(*expr))
		{
			// Do type checking
			if (typeChecker.checkTypeRequirements(*expr))
			{
				// Convert expression to Boogie representation
				auto convResult = ASTBoogieExpressionConverter(m_context, m_currentContract).convert(*expr);
				result.expr = convResult.expr;
				result.exprStr = exprStr;
				result.exprSol = expr;
				result.tccs = convResult.tccs;
				result.ocs = convResult.ocs;

				// Report unsupported cases (side effects)
				if (!convResult.newStatements.empty())
					m_context.reportError(&_node, "Annotation expression introduces intermediate statements");
				if (!convResult.newDecls.empty())
					m_context.reportError(&_node, "Annotation expression introduces intermediate declarations");
				if (!convResult.newConstants.empty())
					m_context.reportError(&_node, "Annotation expression introduces intermediate constants");

			}
		}
	}
	catch (langutil::FatalError const& fe)
	{
		m_context.reportError(&_node, "Error while parsing annotation.");
	}

	// Print errors relating to the expression string
	SourceReferenceFormatter formatter(cerr);
	for (auto const& error: m_context.errorReporter()->errors())
		formatter.printExceptionInformation(*error,
				(error->type() == Error::Type::Warning) ? "Warning" : "solc-verify error");

	// Restore error reporter
	m_context.errorReporter() = originalErrReporter;
	// Add a single error in the original reporter if there were errors
	if (!Error::containsOnlyWarnings(errorList))
	{
		m_context.reportError(&_node, "Error while processing annotation for node");
		return false;
	}
	return true;
}

void ASTBoogieConverter::getExprsFromDocTags(ASTNode const& _node, DocumentedAnnotation const& _annot,
		ASTNode const* _scope, string _tag, vector<BoogieContext::DocTagExpr>& exprs)
{
	for (auto docTag: _annot.docTags)
	{
		if (docTag.first == "notice" && boost::starts_with(docTag.second.content, _tag)) // Find expressions with the given tag
		{
			BoogieContext::DocTagExpr expr;
			if (parseExpr(docTag.second.content.substr(_tag.length() + 1), _node, _scope, expr))
				exprs.push_back(expr);
		}
	}
}

bool ASTBoogieConverter::includeContractInvars(DocumentedAnnotation const& _annot)
{
	for (auto docTag: _annot.docTags)
		if (docTag.first == "notice" && boost::starts_with(docTag.second.content, DOCTAG_CONTRACT_INVARS_INCLUDE))
			return true;

	return false;
}

void ASTBoogieConverter::addModifiesSpecs(FunctionDefinition const& _node, boogie::ProcDeclRef procDecl)
{
	// Modifies specifier
	struct ModSpec {
		boogie::Expr::Ref cond; // Condition
		boogie::Expr::Ref idx;  // Index / selector

		ModSpec() {}
		ModSpec(boogie::Expr::Ref cond, boogie::Expr::Ref idx) : cond(cond), idx(idx) {}
	};

	// Modifies specifier for each variable
	map<Declaration const*, list<ModSpec>> modSpecs;
	bool modAll = false;

	for (auto docTag: _node.annotation().docTags)
	{
		if (docTag.first == "notice" && boost::starts_with(docTag.second.content, DOCTAG_MODIFIES))
		{
			if (boost::algorithm::trim_copy(docTag.second.content) == DOCTAG_MODIFIES_ALL)
			{
				modAll = true;
				continue; // Still parse the rest to catch syntax errors
			}
			size_t targetEnd = docTag.second.content.length();
			boogie::Expr::Ref condExpr = boogie::Expr::lit(true);
			// Check if there is a condition part
			size_t condStart = docTag.second.content.find(DOCTAG_MODIFIES_COND);
			if (condStart != string::npos)
			{
				targetEnd = condStart;
				// Parse the condition
				BoogieContext::DocTagExpr cond;
				if (parseExpr(docTag.second.content.substr(condStart + DOCTAG_MODIFIES_COND.length()), _node, &_node, cond))
					condExpr = cond.expr;
			}
			// Parse the target (variable)
			BoogieContext::DocTagExpr target;
			if (parseExpr(docTag.second.content.substr(DOCTAG_MODIFIES.length() + 1, targetEnd), _node, &_node, target))
			{
				Declaration const* varDecl = nullptr;
				boogie::Expr::Ref indexer = nullptr;
				if (auto id = dynamic_pointer_cast<Identifier const>(target.exprSol))
				{
					varDecl = id->annotation().referencedDeclaration;
				}
				else if (auto ma = dynamic_pointer_cast<MemberAccess const>(target.exprSol))
				{
					varDecl = ma->annotation().referencedDeclaration;
				}
				else if (auto idx = dynamic_pointer_cast<IndexAccess const>(target.exprSol))
				{
					if (auto id = dynamic_cast<Identifier const*>(&idx->baseExpression()))
					{
						varDecl = id->annotation().referencedDeclaration;
						if (auto selExpr = dynamic_pointer_cast<boogie::ArrSelExpr const>(target.expr))
							indexer = selExpr->getIdxs()[0];
						else
							solAssert(false, "Invalid indexer in modifies");
					}
					else
						m_context.reportError(&_node, "Base of indexer in modifies must be identifier");
				}
				else
					m_context.reportError(&_node, "Target of modifies must be identifier or indexer");

				if (varDecl)
					modSpecs[varDecl].push_back(ModSpec(condExpr, indexer));
			}
		}
	}

	if (modAll && !modSpecs.empty())
		m_context.reportWarning(&_node, "Modifies all was given, other modifies specs are ignored");

	if (m_context.modAnalysis() && !_node.isConstructor() && !modAll)
	{
		// Linearized base contracts include the current contract as well
		for (auto contract: m_currentContract->annotation().linearizedBaseContracts)
		{
			for (auto sn: contract->subNodes())
			{
				if (auto varDecl = dynamic_pointer_cast<VariableDeclaration const>(sn))
				{
					if (varDecl->isConstant())
						continue;
					auto varId = boogie::Expr::id(m_context.mapDeclName(*varDecl));
					auto varThis = boogie::Expr::arrsel(varId, m_context.boogieThis()->getRefTo());

					if (varDecl->type()->category() == Type::Category::Struct)
						m_context.reportError(&_node, "Modifies specification is not supported when structures are present.");

					// Build up expression recursively
					boogie::Expr::Ref expr = boogie::Expr::old(varThis);

					for (auto modSpec: modSpecs[varDecl.get()])
					{
						if (modSpec.idx)
						{
							auto selExpr = dynamic_pointer_cast<boogie::ArrSelExpr const>(boogie::Expr::arrsel(expr, modSpec.idx));
							auto writeExpr = ASTBoogieExpressionConverter::selectToUpdate(selExpr,
									boogie::Expr::arrsel(varThis, modSpec.idx));
							expr = boogie::Expr::if_then_else(modSpec.cond, writeExpr, expr);
						}
						else
							expr = boogie::Expr::if_then_else(modSpec.cond, varThis, expr);
					}

					expr = boogie::Expr::eq(varThis, expr);
					string varName = varDecl->name();
					if (m_currentContract->annotation().linearizedBaseContracts.size() > 1)
						varName = contract->name() + "::" + varName;
					procDecl->getEnsures().push_back(boogie::Specification::spec(expr,
							ASTBoogieUtils::createAttrs(_node.location(), "Function might modify '" + varName + "' illegally", *m_context.currentScanner())));
				}
			}
		}
	}
}

void ASTBoogieConverter::processModifier()
{
	if (m_currentModifier < m_currentFunc->modifiers().size()) // We still have modifiers
	{
		auto modifier = m_currentFunc->modifiers()[m_currentModifier];
		auto modifierDecl = dynamic_cast<ModifierDefinition const*>(modifier->name()->annotation().referencedDeclaration);

		if (modifierDecl)
		{
			m_context.pushExtraScope(modifierDecl, toString(modifier->id()));

			string oldReturnLabel = m_currentReturnLabel;
			m_currentReturnLabel = "$return" + to_string(m_nextReturnLabelId);
			++m_nextReturnLabelId;
			m_currentBlocks.top()->addStmt(boogie::Stmt::comment("Inlined modifier " + modifierDecl->name() + " starts here"));

			// Introduce and assign local variables for modifier arguments
			if (modifier->arguments())
			{
				for (unsigned long i = 0; i < modifier->arguments()->size(); ++i)
				{
					auto paramDecls = modifierDecl->parameters()[i];
					boogie::Decl::Ref modifierParam = boogie::Decl::variable(m_context.mapDeclName(*paramDecls),
							m_context.toBoogieType(modifierDecl->parameters()[i]->annotation().type, paramDecls.get()));
					m_localDecls.push_back(modifierParam);
					boogie::Expr::Ref modifierArg = convertExpression(*modifier->arguments()->at(i));
					m_currentBlocks.top()->addStmt(boogie::Stmt::assign(boogie::Expr::id(modifierParam->getName()), modifierArg));
				}
			}
			modifierDecl->body().accept(*this);
			m_currentBlocks.top()->addStmt(boogie::Stmt::label(m_currentReturnLabel));
			m_currentBlocks.top()->addStmt(boogie::Stmt::comment("Inlined modifier " + modifierDecl->name() + " ends here"));
			m_currentReturnLabel = oldReturnLabel;
			m_context.popExtraScope();
		}
		else if (dynamic_cast<ContractDefinition const*>(modifier->name()->annotation().referencedDeclaration))
			m_context.reportError(modifier.get(), "Base constructor call is not supported as modifier invocation");
		else
			m_context.reportError(modifier.get(), "Unsupported modifier invocation");
	}
	else if (m_currentFunc->isImplemented()) // We reached the function
	{
		string oldReturnLabel = m_currentReturnLabel;
		m_currentReturnLabel = "$return" + to_string(m_nextReturnLabelId);
		++m_nextReturnLabelId;
		m_currentBlocks.top()->addStmt(boogie::Stmt::comment("Function body starts here"));
		m_currentFunc->body().accept(*this);
		m_currentBlocks.top()->addStmt(boogie::Stmt::label(m_currentReturnLabel));
		m_currentBlocks.top()->addStmt(boogie::Stmt::comment("Function body ends here"));
		m_currentReturnLabel = oldReturnLabel;
	}
}

ASTBoogieConverter::ASTBoogieConverter(BoogieContext& context) :
				m_context(context),
				m_currentContract(nullptr),
				m_currentFunc(nullptr),
				m_currentModifier(0),
				m_currentRet(nullptr),
				m_nextReturnLabelId(0)
{
}

// ---------------------------------------------------------------------------
//         Visitor methods for top-level nodes and declarations
// ---------------------------------------------------------------------------

bool ASTBoogieConverter::visit(SourceUnit const& _node)
{
	rememberScope(_node);

	// Boogie programs are flat, source units do not appear explicitly
	m_context.addGlobalComment("\n------- Source: " + _node.annotation().path + " -------");
	return true; // Simply apply visitor recursively
}

bool ASTBoogieConverter::visit(PragmaDirective const& _node)
{
	rememberScope(_node);

	// Pragmas are only included as comments
	m_context.addGlobalComment("Pragma: " + boost::algorithm::join(_node.literals(), ""));
	return false;
}

bool ASTBoogieConverter::visit(ImportDirective const& _node)
{
	rememberScope(_node);
	return false;
}

bool ASTBoogieConverter::visit(ContractDefinition const& _node)
{
	rememberScope(_node);

	m_currentContract = &_node;
	// Boogie programs are flat, contracts do not appear explicitly
	m_context.addGlobalComment("\n------- Contract: " + _node.name() + " -------");

	// Process contract invariants
	m_context.currentContractInvars().clear();
	m_context.currentSumDecls().clear();

	std::vector<BoogieContext::DocTagExpr> invars;
	getExprsFromDocTags(_node, _node.annotation(), &_node, DOCTAG_CONTRACT_INVAR, invars);
	for (auto invar: invars)
	{
		m_context.addGlobalComment("Contract invariant: " + invar.exprStr);
		m_context.currentContractInvars().push_back(invar);
	}

	// Add new shadow variables for sum
	for (auto sumDecl: m_context.currentSumDecls())
	{
		m_context.addGlobalComment("Shadow variable for sum over '" + sumDecl.first->name() + "'");
		m_context.addDecl(
					boogie::Decl::variable(m_context.mapDeclName(*sumDecl.first) + ASTBoogieUtils::BOOGIE_SUM,
							ASTBoogieUtils::mappingType(m_context.addressType(),
							m_context.toBoogieType(sumDecl.second, sumDecl.first))));
	}

	// Process inheritance specifiers (not included in subNodes)
	for (auto ispec: _node.baseContracts())
		ispec->accept(*this);

	// Collect state variable initializers first, must be done for
	// base class members as well
	m_stateVarInitializers.clear();
	m_stateVarsToInitialize.clear();
	for (auto contract: _node.annotation().linearizedBaseContracts)
		for (auto sn: contract->subNodes())
			if (auto sv = dynamic_pointer_cast<VariableDeclaration const>(sn))
				checkForInitializer(*sv);

	// Process subnodes
	for (auto sn: _node.subNodes())
		sn->accept(*this);

	// If no constructor exists, create an implicit one
	bool hasConstructor = false;
	for (auto sn: _node.subNodes())
	{
		if (auto fn = dynamic_pointer_cast<FunctionDefinition const>(sn))
		{
			if (fn->isConstructor())
			{
				hasConstructor = true;
				break;
			}
		}
	}
	if (!hasConstructor)
		createImplicitConstructor(_node);

	return false;
}

bool ASTBoogieConverter::visit(InheritanceSpecifier const& _node)
{
	rememberScope(_node);

	// TODO: calling constructor of superclass?
	// Boogie programs are flat, inheritance does not appear explicitly
	m_context.addGlobalComment("Inherits from: " + boost::algorithm::join(_node.name().namePath(), "#"));
	if (_node.arguments() && _node.arguments()->size() > 0)
	{
		m_context.reportError(&_node, "Arguments for base constructor in inheritance list is not supported");
	}
	return false;
}

bool ASTBoogieConverter::visit(UsingForDirective const& _node)
{
	rememberScope(_node);

	// Nothing to do with using for directives, calls to functions are resolved in the AST
	string libraryName = _node.libraryName().annotation().type->toString();
	string typeName = _node.typeName() ? _node.typeName()->annotation().type->toString() : "*";
	m_context.addGlobalComment("Using " + libraryName + " for " + typeName);
	return false;
}

bool ASTBoogieConverter::visit(StructDefinition const& _node)
{
	rememberScope(_node);

	m_context.addGlobalComment("\n------- Struct: " + _node.name() + "-------");
	// Define type for memory
	boogie::TypeDeclRef structMemType = m_context.getStructType(&_node, DataLocation::Memory);
	// Create mappings for each member (only for memory structs)
	for (auto member: _node.members())
	{
		boogie::TypeDeclRef memberType = nullptr;
		// Nested structures
		if (member->type()->category() == Type::Category::Struct)
		{
			auto structTp = dynamic_cast<StructType const*>(member->type());
			memberType = m_context.getStructType(&structTp->structDefinition(), DataLocation::Memory);
		}
		else // Other types
			memberType = m_context.toBoogieType(member->type(), member.get());
		// TODO: array members?

		auto attrs = ASTBoogieUtils::createAttrs(member->location(), member->name(), *m_context.currentScanner());
		auto memberDecl = boogie::Decl::variable(m_context.mapDeclName(*member),
				ASTBoogieUtils::mappingType(structMemType, memberType));
		memberDecl->addAttrs(attrs);
		m_context.addDecl(memberDecl);
	}

	return false;
}

bool ASTBoogieConverter::visit(EnumDefinition const& _node)
{
	rememberScope(_node);
	m_context.addGlobalComment("Enum definition " + _node.name() + " mapped to int");
	return false;
}

bool ASTBoogieConverter::visit(EnumValue const& _node)
{
	rememberScope(_node);

	solAssert(false, "Unhandled node: EnumValue");
	return false;
}

bool ASTBoogieConverter::visit(ParameterList const& _node)
{
	rememberScope(_node);

	solAssert(false, "Unhandled node: ParameterList");
	return false;
}

bool ASTBoogieConverter::visit(FunctionDefinition const& _node)
{
	rememberScope(_node);

	// Solidity functions are mapped to Boogie procedures
	m_currentFunc = &_node;

	// Type to pass around
	TypePointer tp_uint256 = TypeProvider::integer(256, IntegerType::Modifier::Unsigned);

	// Input parameters
	vector<boogie::Binding> params {
		// Globally available stuff
		{m_context.boogieThis()->getRefTo(), m_context.boogieThis()->getType() }, // this
		{m_context.boogieMsgSender()->getRefTo(), m_context.boogieMsgSender()->getType() }, // msg.sender
		{m_context.boogieMsgValue()->getRefTo(), m_context.boogieMsgValue()->getType() } // msg.value
	};
	// Add original parameters of the function
	for (auto par: _node.parameters())
	{
		params.push_back({boogie::Expr::id(m_context.mapDeclName(*par)), m_context.toBoogieType(par->type(), par.get())});
		if (par->type()->category() == Type::Category::Array) // Array length
			params.push_back({boogie::Expr::id(m_context.mapDeclName(*par) + ASTBoogieUtils::BOOGIE_LENGTH), m_context.intType(256) });
	}

	// Return values
	vector<boogie::Binding> rets;
	vector<boogie::Expr::Ref> retIds;
	for (auto ret: _node.returnParameters())
	{
		if (ret->type()->category() == Type::Category::Array)
		{
			auto arrType = dynamic_cast<ArrayType const*>(ret->type());
			if (!arrType->isString())
			{
				m_context.reportError(&_node, "Arrays are not supported as return values");
				return false;
			}
		}
		boogie::Expr::Ref retId = boogie::Expr::id(m_context.mapDeclName(*ret));
		boogie::TypeDeclRef retType = m_context.toBoogieType(ret->type(), ret.get());
		retIds.push_back(retId);
		rets.push_back({retId, retType});
	}

	// Boogie treats return as an assignment to the return variable(s)
	if (_node.returnParameters().empty())
		m_currentRet = nullptr;
	else if (_node.returnParameters().size() == 1)
		m_currentRet = retIds[0];
	else
		m_currentRet = boogie::Expr::tuple(retIds);

	// Convert function body, collect result
	m_localDecls.clear();
	// Create new empty block
	m_currentBlocks.push(boogie::Block::block());
	// Include constructor preamble
	if (_node.isConstructor())
		constructorPreamble(_node);
	// Payable functions should handle msg.value
	if (_node.isPayable())
	{
		boogie::Expr::Ref this_bal = boogie::Expr::arrsel(m_context.boogieBalance()->getRefTo(), m_context.boogieThis()->getRefTo());
		boogie::Expr::Ref msg_val = m_context.boogieMsgValue()->getRefTo();
		// balance[this] += msg.value
		if (m_context.encoding() == BoogieContext::Encoding::MOD)
		{
			m_currentBlocks.top()->addStmt(boogie::Stmt::assume(ASTBoogieUtils::getTCCforExpr(this_bal, tp_uint256)));
			m_currentBlocks.top()->addStmt(boogie::Stmt::assume(ASTBoogieUtils::getTCCforExpr(msg_val, tp_uint256)));
		}
		auto addResult = ASTBoogieUtils::encodeArithBinaryOp(m_context, nullptr, Token::Add, this_bal, msg_val, 256, false);
		if (m_context.overflow())
		{
			m_currentBlocks.top()->addStmt(boogie::Stmt::comment("Implicit assumption that balances cannot overflow"));
			m_currentBlocks.top()->addStmt(boogie::Stmt::assume(addResult.cc));
		}
		m_currentBlocks.top()->addStmt(boogie::Stmt::assign(
				m_context.boogieBalance()->getRefTo(),
					boogie::Expr::arrupd(m_context.boogieBalance()->getRefTo(), m_context.boogieThis()->getRefTo(), addResult.expr)));
	}

	// Modifiers need to be inlined
	if (_node.modifiers().empty())
	{
		if (_node.isImplemented())
		{
			string retLabel = "$return" + to_string(m_nextReturnLabelId);
			++m_nextReturnLabelId;
			m_currentReturnLabel = retLabel;
			_node.body().accept(*this);
			m_currentBlocks.top()->addStmt(boogie::Stmt::label(retLabel));
		}
	}
	else
	{
		m_currentModifier = 0;
		processModifier();
	}

	vector<boogie::Block::Ref> blocks;
	if (!m_currentBlocks.top()->getStatements().empty())
		blocks.push_back(m_currentBlocks.top());
	m_currentBlocks.pop();
	solAssert(m_currentBlocks.empty(), "Non-empty stack of blocks at the end of function.");

	// Get the name of the function
	string funcName = _node.isConstructor() ?
			ASTBoogieUtils::getConstructorName(m_currentContract) :
			m_context.mapDeclName(_node);

	// Create the procedure
	auto procDecl = boogie::Decl::procedure(funcName, params, rets, m_localDecls, blocks);

	// Overflow condition for the code comes first because if there are more errors, this one gets reported
	if (m_context.overflow())
	{
		auto noOverflow = boogie::Expr::not_(boogie::Expr::id(ASTBoogieUtils::VERIFIER_OVERFLOW));
		procDecl->getRequires().push_back(boogie::Specification::spec(noOverflow,
				ASTBoogieUtils::createAttrs(_node.location(), "An overflow can occur before calling function", *m_context.currentScanner())));
		procDecl->getEnsures().push_back(boogie::Specification::spec(noOverflow,
				ASTBoogieUtils::createAttrs(_node.location(), "Function can terminate with overflow", *m_context.currentScanner())));
	}

	// add invariants as pre/postconditions for: public functions and if explicitly requested
	if (_node.isPublic() || includeContractInvars(_node.annotation()))
	{
		for (auto invar: m_context.currentContractInvars())
		{
			for (auto oc: invar.ocs)
			{
				procDecl->getRequires().push_back(boogie::Specification::spec(oc,
					ASTBoogieUtils::createAttrs(_node.location(), "Overflow in computation of invariant '" + invar.exprStr + "' when entering function.", *m_context.currentScanner())));
				procDecl->getEnsures().push_back(boogie::Specification::spec(oc,
					ASTBoogieUtils::createAttrs(_node.location(), "Overflow in computation of invariant '" + invar.exprStr + "' at end of function.", *m_context.currentScanner())));
			}
			for (auto tcc: invar.tccs)
			{
				procDecl->getRequires().push_back(boogie::Specification::spec(tcc,
					ASTBoogieUtils::createAttrs(_node.location(), "Variables in invariant '" + invar.exprStr + "' might be out of range when entering function.", *m_context.currentScanner())));
				procDecl->getEnsures().push_back(boogie::Specification::spec(tcc,
					ASTBoogieUtils::createAttrs(_node.location(), "Variables in invariant '" + invar.exprStr + "' might be out of range at end of function.", *m_context.currentScanner())));
			}
			if (!_node.isConstructor())
			{
				procDecl->getRequires().push_back(boogie::Specification::spec(invar.expr,
					ASTBoogieUtils::createAttrs(_node.location(), "Invariant '" + invar.exprStr + "' might not hold when entering function.", *m_context.currentScanner())));
			}
			procDecl->getEnsures().push_back(boogie::Specification::spec(invar.expr,
					ASTBoogieUtils::createAttrs(_node.location(), "Invariant '" + invar.exprStr + "' might not hold at end of function.", *m_context.currentScanner())));
		}
	}

	if (!_node.isPublic()) // Non-public functions: inline
		procDecl->addAttr(boogie::Attr::attr("inline", 1));

	// Add other pre/postconditions
	std::vector<BoogieContext::DocTagExpr> preconds;
	getExprsFromDocTags(_node, _node.annotation(), &_node, DOCTAG_PRECOND, preconds);
	for (auto pre: preconds)
	{
		procDecl->getRequires().push_back(boogie::Specification::spec(pre.expr,
							ASTBoogieUtils::createAttrs(_node.location(), "Precondition '" + pre.exprStr + "' might not hold when entering function.", *m_context.currentScanner())));
		for (auto tcc: pre.tccs)
		{
			procDecl->getRequires().push_back(boogie::Specification::spec(tcc,
				ASTBoogieUtils::createAttrs(_node.location(), "Variables in precondition '" + pre.exprStr + "' might be out of range when entering function.", *m_context.currentScanner())));
		}
		for (auto oc: pre.ocs)
		{
			procDecl->getRequires().push_back(boogie::Specification::spec(oc,
						ASTBoogieUtils::createAttrs(_node.location(), "Overflow in computation of precondition '" + pre.exprStr + "' when entering function.", *m_context.currentScanner())));
		}
	}
	std::vector<BoogieContext::DocTagExpr> postconds;
	getExprsFromDocTags(_node, _node.annotation(), &_node, DOCTAG_POSTCOND, postconds);
	for (auto post: postconds)
	{
		procDecl->getEnsures().push_back(boogie::Specification::spec(post.expr,
							ASTBoogieUtils::createAttrs(_node.location(), "Postcondition '" + post.exprStr + "' might not hold at end of function.", *m_context.currentScanner())));
		for (auto tcc: post.tccs)
		{
			procDecl->getEnsures().push_back(boogie::Specification::spec(tcc,
						ASTBoogieUtils::createAttrs(_node.location(), "Variables in postcondition '" + post.exprStr + "' might be out of range at end of function.", *m_context.currentScanner())));
		}
		for (auto oc: post.ocs)
		{
			procDecl->getEnsures().push_back(boogie::Specification::spec(oc,
							ASTBoogieUtils::createAttrs(_node.location(), "Overflow in computation of postcondition '" + post.exprStr + "' at end of function.", *m_context.currentScanner())));
		}
	}
	// TODO: check that no new sum variables were introduced

	// Modifies specifications
	addModifiesSpecs(_node, procDecl);

	dev::solidity::ASTString traceabilityName = m_currentContract->name() + "::" + (_node.isConstructor() ? "[constructor]" : _node.name().c_str());
	procDecl->addAttrs(ASTBoogieUtils::createAttrs(_node.location(), traceabilityName, *m_context.currentScanner()));
	string funcType = _node.visibility() == Declaration::Visibility::External ? "" : " : " + _node.type()->toString();
	m_context.addGlobalComment("\nFunction: " + _node.name() + funcType);
	m_context.addDecl(procDecl);
	return false;
}

void ASTBoogieConverter::checkForInitializer(VariableDeclaration const& _node)
{
	// Constants are inlined
	if (_node.isConstant())
		return;

	if (_node.value())
	{
		// Initialization is saved for the constructor
		// A new block is introduced to collect side effects of the initializer expression
		m_currentBlocks.push(boogie::Block::block());
		m_localDecls.clear();
		boogie::Expr::Ref initExpr = convertExpression(*_node.value());
		m_stateVarInitTmpDecls.insert(end(m_stateVarInitTmpDecls), begin(m_localDecls), end(m_localDecls));

		if (m_context.isBvEncoding() && ASTBoogieUtils::isBitPreciseType(_node.annotation().type))
		{
			initExpr = ASTBoogieUtils::checkImplicitBvConversion(initExpr, _node.value()->annotation().type, _node.annotation().type, m_context);
		}
		m_stateVarInitializers.insert(end(m_stateVarInitializers), begin(*m_currentBlocks.top()), end(*m_currentBlocks.top()));

		m_currentBlocks.pop();
		m_stateVarInitializers.push_back(boogie::Stmt::assign(boogie::Expr::id(m_context.mapDeclName(_node)),
				boogie::Expr::arrupd(
						boogie::Expr::id(m_context.mapDeclName(_node)),
						m_context.boogieThis()->getRefTo(),
						initExpr)));
	} else {
		// Use implicit default value (will generate later, once we are in scope of constructor)
		m_stateVarsToInitialize.push_back(&_node);
	}
}

bool ASTBoogieConverter::visit(VariableDeclaration const& _node)
{
	rememberScope(_node);

	// Non-state variables should be handled in the VariableDeclarationStatement
	solAssert(_node.isStateVariable(), "Non-state variable appearing in VariableDeclaration");

	// Initializers are collected by the visitor for ContractDefinition

	// Constants are inlined
	if (_node.isConstant())
		return false;

	m_context.addGlobalComment("\nState variable: " + _node.name() + " : " + _node.type()->toString());
	// State variables are represented as maps from address to their type
	auto varDecl = boogie::Decl::variable(m_context.mapDeclName(_node),
			ASTBoogieUtils::mappingType(
					m_context.addressType(),
					m_context.toBoogieType(_node.type(), &_node)));
	varDecl->addAttrs(ASTBoogieUtils::createAttrs(_node.location(), _node.name(), *m_context.currentScanner()));
	m_context.addDecl(varDecl);

	// Arrays require an extra variable for their length
	if (_node.type()->category() == Type::Category::Array)
	{
		m_context.addDecl(
				boogie::Decl::variable(m_context.mapDeclName(_node) + ASTBoogieUtils::BOOGIE_LENGTH,
						ASTBoogieUtils::mappingType(
								m_context.addressType(),
								m_context.intType(256))));
	}
	return false;
}

bool ASTBoogieConverter::visit(ModifierDefinition const& _node)
{
	rememberScope(_node);

	// Modifier definitions do not appear explicitly, but are instead inlined to functions
	return false;
}

bool ASTBoogieConverter::visit(ModifierInvocation const& _node)
{
	rememberScope(_node);

	solAssert(false, "Unhandled node: ModifierInvocation");
	return false;
}

bool ASTBoogieConverter::visit(EventDefinition const& _node)
{
	rememberScope(_node);

	m_context.reportWarning(&_node, "Ignored event definition");
	return false;
}

bool ASTBoogieConverter::visit(ElementaryTypeName const& _node)
{
	rememberScope(_node);

	solAssert(false, "Unhandled node: ElementaryTypeName");
	return false;
}

bool ASTBoogieConverter::visit(UserDefinedTypeName const& _node)
{
	rememberScope(_node);

	solAssert(false, "Unhandled node: UserDefinedTypeName");
	return false;
}

bool ASTBoogieConverter::visit(FunctionTypeName const& _node)
{
	rememberScope(_node);

	solAssert(false, "Unhandled node: FunctionTypeName");
	return false;
}

bool ASTBoogieConverter::visit(Mapping const& _node)
{
	rememberScope(_node);

	solAssert(false, "Unhandled node: Mapping");
	return false;
}

bool ASTBoogieConverter::visit(ArrayTypeName const& _node)
{
	rememberScope(_node);

	solAssert(false, "Unhandled node: ArrayTypeName");
	return false;
}

// ---------------------------------------------------------------------------
//                     Visitor methods for statements
// ---------------------------------------------------------------------------

bool ASTBoogieConverter::visit(InlineAssembly const& _node)
{
	rememberScope(_node);

	m_context.reportError(&_node, "Inline assembly is not supported");
	return false;
}

bool ASTBoogieConverter::visit(Block const& _node)
{
	rememberScope(_node);

	// Simply apply visitor recursively, compound statements will create new blocks when required
	return true;
}

bool ASTBoogieConverter::visit(PlaceholderStatement const& _node)
{
	rememberScope(_node);

	m_currentModifier++; // Go one level deeper
	processModifier();   // Process the body
	m_currentModifier--; // We are back

	return false;
}

bool ASTBoogieConverter::visit(IfStatement const& _node)
{
	rememberScope(_node);

	// Get condition recursively
	boogie::Expr::Ref cond = convertExpression(_node.condition());

	// Get true branch recursively
	m_currentBlocks.push(boogie::Block::block());
	_node.trueStatement().accept(*this);
	boogie::Block::ConstRef thenBlock = m_currentBlocks.top();
	m_currentBlocks.pop();

	// Get false branch recursively (might not exist)
	boogie::Block::ConstRef elseBlock = nullptr;
	if (_node.falseStatement())
	{
		m_currentBlocks.push(boogie::Block::block());
		_node.falseStatement()->accept(*this);
		elseBlock = m_currentBlocks.top();
		m_currentBlocks.pop();
	}

	m_currentBlocks.top()->addStmt(boogie::Stmt::ifelse(cond, thenBlock, elseBlock));
	return false;
}

bool ASTBoogieConverter::visit(WhileStatement const& _node)
{
	rememberScope(_node);

	if (_node.isDoWhile())
	{
		m_context.reportError(&_node, "Do-while loops are not supported");
		return false;
	}

	string oldContinueLabel = m_currentContinueLabel;
	m_currentContinueLabel = "$continue" + toString(_node.id());

	// Get condition recursively
	boogie::Expr::Ref cond = convertExpression(_node.condition());

	// Get if branch recursively
	m_currentBlocks.push(boogie::Block::block());
	_node.body().accept(*this);
	m_currentBlocks.top()->addStmt(boogie::Stmt::label(m_currentContinueLabel));
	boogie::Block::ConstRef body = m_currentBlocks.top();
	m_currentBlocks.pop();
	m_currentContinueLabel = oldContinueLabel;

	std::vector<boogie::Specification::Ref> invars;

	// No overflow in code
	if (m_context.overflow())
	{
		invars.push_back(boogie::Specification::spec(
				boogie::Expr::not_(boogie::Expr::id(ASTBoogieUtils::VERIFIER_OVERFLOW)),
				ASTBoogieUtils::createAttrs(_node.location(), "No overflow", *m_context.currentScanner())
		));
	}

	std::vector<BoogieContext::DocTagExpr> loopInvars;
	getExprsFromDocTags(_node, _node.annotation(), scope(), DOCTAG_LOOP_INVAR, loopInvars);
	if (includeContractInvars(_node.annotation()))
		loopInvars.insert(end(loopInvars), begin(m_context.currentContractInvars()), end(m_context.currentContractInvars()));
	for (auto invar: loopInvars)
	{
		for (auto tcc: invar.tccs)
			invars.push_back(boogie::Specification::spec(tcc,
					ASTBoogieUtils::createAttrs(_node.location(), "variables in range for '" + invar.exprStr + "'", *m_context.currentScanner())));

		for (auto oc: invar.ocs)
			invars.push_back(boogie::Specification::spec(oc,
					ASTBoogieUtils::createAttrs(_node.location(), "no overflow in '" + invar.exprStr + "'", *m_context.currentScanner())));

		invars.push_back(boogie::Specification::spec(invar.expr, ASTBoogieUtils::createAttrs(_node.location(), invar.exprStr, *m_context.currentScanner())));
	}
	// TODO: check that invariants did not introduce new sum variables

	m_currentBlocks.top()->addStmt(boogie::Stmt::while_(cond, body, invars));

	return false;
}

bool ASTBoogieConverter::visit(ForStatement const& _node)
{
	rememberScope(_node);

	// Boogie does not have a for statement, therefore it is transformed
	// into a while statement in the following way:
	//
	// for (initExpr; cond; loopExpr) { body }
	//
	// initExpr; while (cond) { body; loopExpr }

	string oldContinueLabel = m_currentContinueLabel;
	m_currentContinueLabel = "$continue" + toString(_node.id());

	// Get initialization recursively (adds statement to current block)
	m_currentBlocks.top()->addStmt(boogie::Stmt::comment("The following while loop was mapped from a for loop"));
	if (_node.initializationExpression())
	{
		m_currentBlocks.top()->addStmt(boogie::Stmt::comment("Initialization"));
		_node.initializationExpression()->accept(*this);
	}

	// Get condition recursively
	boogie::Expr::Ref cond = _node.condition() ? convertExpression(*_node.condition()) : nullptr;

	// Get body recursively
	m_currentBlocks.push(boogie::Block::block());
	m_currentBlocks.top()->addStmt(boogie::Stmt::comment("Body"));
	_node.body().accept(*this);
	m_currentBlocks.top()->addStmt(boogie::Stmt::label(m_currentContinueLabel));
	// Include loop expression at the end of body
	if (_node.loopExpression())
	{
		m_currentBlocks.top()->addStmt(boogie::Stmt::comment("Loop expression"));
		_node.loopExpression()->accept(*this); // Adds statements to current block
	}
	boogie::Block::ConstRef body = m_currentBlocks.top();
	m_currentBlocks.pop();
	m_currentContinueLabel = oldContinueLabel;

	std::vector<boogie::Specification::Ref> invars;

	// No overflow in code
	if (m_context.overflow())
	{
		invars.push_back(boogie::Specification::spec(
				boogie::Expr::not_(boogie::Expr::id(ASTBoogieUtils::VERIFIER_OVERFLOW)),
				ASTBoogieUtils::createAttrs(_node.location(), "No overflow", *m_context.currentScanner())
		));
	}

	std::vector<BoogieContext::DocTagExpr> loopInvars;
	getExprsFromDocTags(_node, _node.annotation(), &_node, DOCTAG_LOOP_INVAR, loopInvars);
	if (includeContractInvars(_node.annotation()))
		loopInvars.insert(end(loopInvars), begin(m_context.currentContractInvars()), end(m_context.currentContractInvars()));
	for (auto invar: loopInvars)
	{
		for (auto tcc: invar.tccs)
			invars.push_back(boogie::Specification::spec(tcc,
					ASTBoogieUtils::createAttrs(_node.location(), "variables in range for '" + invar.exprStr + "'", *m_context.currentScanner())));

		for (auto oc: invar.ocs)
			invars.push_back(boogie::Specification::spec(oc,
					ASTBoogieUtils::createAttrs(_node.location(), "no overflow in '" + invar.exprStr + "'", *m_context.currentScanner())));

		invars.push_back(boogie::Specification::spec(invar.expr, ASTBoogieUtils::createAttrs(_node.location(), invar.exprStr, *m_context.currentScanner())));
	}
	// TODO: check that invariants did not introduce new sum variables

	m_currentBlocks.top()->addStmt(boogie::Stmt::while_(cond, body, invars));

	return false;
}

bool ASTBoogieConverter::visit(Continue const& _node)
{
	rememberScope(_node);
	m_currentBlocks.top()->addStmt(boogie::Stmt::goto_({m_currentContinueLabel}));
	return false;
}

bool ASTBoogieConverter::visit(Break const& _node)
{
	rememberScope(_node);

	m_currentBlocks.top()->addStmt(boogie::Stmt::break_());
	return false;
}

bool ASTBoogieConverter::visit(Return const& _node)
{
	rememberScope(_node);

	if (_node.expression() != nullptr)
	{
		// Get rhs recursively
		boogie::Expr::Ref rhs = convertExpression(*_node.expression());

		if (m_context.isBvEncoding())
		{
			auto rhsType = _node.expression()->annotation().type;

			// Return type
			TypePointer returnType = nullptr;
			auto const& returnParams = m_currentFunc->returnParameters();
			if (returnParams.size() > 1)
			{
				std::vector<TypePointer> elems;
				for (auto p: returnParams)
					elems.push_back(p->annotation().type);
				returnType = TypeProvider::tuple(elems);
			}
			else
				returnType = returnParams[0]->annotation().type;

			rhs = ASTBoogieUtils::checkImplicitBvConversion(rhs, rhsType, returnType, m_context);
		}

		// LHS of assignment should already be known (set by the enclosing FunctionDefinition)
		boogie::Expr::Ref lhs = m_currentRet;

		// First create an assignment, and then an empty return
		m_currentBlocks.top()->addStmt(boogie::Stmt::assign(lhs, rhs));
	}
	m_currentBlocks.top()->addStmt(boogie::Stmt::goto_({m_currentReturnLabel}));
	return false;
}

bool ASTBoogieConverter::visit(Throw const& _node)
{
	rememberScope(_node);

	m_currentBlocks.top()->addStmt(boogie::Stmt::assume(boogie::Expr::lit(false)));
	return false;
}

bool ASTBoogieConverter::visit(EmitStatement const& _node)
{
	rememberScope(_node);

	m_context.reportWarning(&_node, "Ignored emit statement");
	return false;
}

bool ASTBoogieConverter::visit(VariableDeclarationStatement const& _node)
{
	rememberScope(_node);

	auto const& declarations = _node.declarations();
	auto initialValue = _node.initialValue();

	for (auto decl: declarations)
	{
		// Decl can be null, e.g., var (x,,) = (1,2,3)
		// In this case we just ignore it
		if (decl != nullptr)
		{
			solAssert(decl->isLocalVariable(), "Non-local variable appearing in VariableDeclarationStatement");
			// Boogie requires local variables to be declared at the beginning of the procedure
			auto varDecl = boogie::Decl::variable(
					m_context.mapDeclName(*decl),
					m_context.toBoogieType(decl->type(), decl.get()));
			varDecl->addAttrs(ASTBoogieUtils::createAttrs(decl->location(), decl->name(), *m_context.currentScanner()));
			m_localDecls.push_back(varDecl);
		}
	}

	// Convert initial value into an assignment statement
	if (initialValue)
	{
		auto initalValueType = initialValue->annotation().type;

		// Get expression recursively
		boogie::Expr::Ref rhs = convertExpression(*initialValue);

		if (declarations.size() == 1)
		{
			// One return value, simple
			auto decl = declarations[0];
			auto declType = decl->type();
			if (m_context.isBvEncoding() && ASTBoogieUtils::isBitPreciseType(declType))
				rhs = ASTBoogieUtils::checkImplicitBvConversion(rhs, initalValueType, declType, m_context);
			m_currentBlocks.top()->addStmt(boogie::Stmt::assign(
					boogie::Expr::id(m_context.mapDeclName(*decl)),
					rhs));
		}
		else
		{
			auto initTupleType = dynamic_cast<TupleType const*>(initalValueType);
			auto rhsTuple = dynamic_pointer_cast<boogie::TupleExpr const>(rhs);
			for (size_t i = 0; i < declarations.size(); ++ i)
			{
				// One return value, simple
				auto decl = declarations[i];
				if (decl != nullptr)
				{
					auto declType = decl->type();
					auto exprType = initTupleType->components()[i];
					auto rhs_i = rhsTuple->elements()[i];
					if (m_context.isBvEncoding() && ASTBoogieUtils::isBitPreciseType(declType))
						rhs_i = ASTBoogieUtils::checkImplicitBvConversion(rhs_i, exprType, declType, m_context);
					m_currentBlocks.top()->addStmt(boogie::Stmt::assign(
							boogie::Expr::id(m_context.mapDeclName(*decl)),
							rhs_i));
					}
			}
		}
	}
	// Otherwise initialize with default value
	else
	{
		for (auto declNode: _node.declarations())
		{
			boogie::Expr::Ref defaultVal = defaultValue(declNode->type(), m_context);
			if (defaultVal)
			{
				m_currentBlocks.top()->addStmt(boogie::Stmt::assign(
									boogie::Expr::id(m_context.mapDeclName(*declNode)),
									defaultVal));
			}
			else
			{
				// TODO: maybe this should be an error
				m_context.reportWarning(declNode.get(), "Boogie: Unhandled default value, verification might fail.");
			}
		}
	}
	return false;
}

bool ASTBoogieConverter::visit(ExpressionStatement const& _node)
{
	rememberScope(_node);

	// Some expressions have specific statements in Boogie

	// Assignment
	if (dynamic_cast<Assignment const*>(&_node.expression()))
	{
		convertExpression(_node.expression());
		return false;
	}

	// Function call
	if (dynamic_cast<FunctionCall const*>(&_node.expression()))
	{
		convertExpression(_node.expression());
		return false;
	}

	// Increment, decrement
	if (auto unaryExpr = dynamic_cast<UnaryOperation const*>(&_node.expression()))
	{
		if (unaryExpr->getOperator() == Token::Inc || unaryExpr->getOperator() == Token::Dec)
		{
			convertExpression(_node.expression());
			return false;
		}
	}

	// Other statements are assigned to a temp variable because Boogie
	// does not allow stand alone expressions

	boogie::Expr::Ref expr = convertExpression(_node.expression());
	boogie::Decl::Ref tmpVar = boogie::Decl::variable("tmpVar" + to_string(_node.id()),
			m_context.toBoogieType(_node.expression().annotation().type, &_node));
	m_localDecls.push_back(tmpVar);
	m_currentBlocks.top()->addStmt(boogie::Stmt::comment("Assignment to temp variable introduced because Boogie does not support stand alone expressions"));
	m_currentBlocks.top()->addStmt(boogie::Stmt::assign(boogie::Expr::id(tmpVar->getName()), expr));
	return false;
}


bool ASTBoogieConverter::visitNode(ASTNode const& _node)
{
	rememberScope(_node);

	solAssert(false, "Unhandled node (unknown)");
	return true;
}

}
}
