#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <libsolidity/analysis/NameAndTypeResolver.h>
#include <libsolidity/analysis/TypeChecker.h>
#include <libsolidity/ast/ASTBoogieConverter.h>
#include <libsolidity/ast/ASTBoogieExpressionConverter.h>
#include <libsolidity/ast/ASTBoogieUtils.h>
#include <libsolidity/parsing/Parser.h>
#include <libsolidity/interface/SourceReferenceFormatter.h>

using namespace std;
using namespace dev;
using namespace dev::solidity;

namespace dev
{
namespace solidity
{

const string ASTBoogieConverter::DOCTAG_CONTRACT_INVAR = "invariant";
const string ASTBoogieConverter::DOCTAG_CONTRACT_INVARS_INCLUDE = "{contractInvariants}";
const string ASTBoogieConverter::DOCTAG_LOOP_INVAR = "invariant";
const string ASTBoogieConverter::DOCTAG_PRECOND = "precondition";
const string ASTBoogieConverter::DOCTAG_POSTCOND = "postcondition";

void ASTBoogieConverter::addGlobalComment(string str)
{
	m_context.program().getDeclarations().push_back(smack::Decl::comment("", str));
}

const smack::Expr* ASTBoogieConverter::convertExpression(Expression const& _node)
{
	ASTBoogieExpressionConverter::Result result = ASTBoogieExpressionConverter(m_context).convert(_node);

	for (auto d : result.newDecls) { m_localDecls.push_back(d); }
	for (auto tcc : result.tccs) { m_currentBlocks.top()->addStmt(smack::Stmt::assume(tcc)); }
	for (auto s : result.newStatements) { m_currentBlocks.top()->addStmt(s); }
	for (auto c : result.newConstants)
	{
		bool alreadyDefined = false;
		for (auto d : m_context.program().getDeclarations())
		{
			if (d->getName() == c->getName())
			{
				// TODO: check that other fields are equal
				alreadyDefined = true;
				break;
			}
		}
		if (!alreadyDefined) m_context.program().getDeclarations().push_back(c);
	}

	return result.expr;
}

const smack::Expr* ASTBoogieConverter::defaultValue(TypePointer type) {

	const smack::Stmt* defaultValue(TypePointer type);
	switch (type->category()) {
	case Type::Category::Integer: {
		// Zero
		bool bitPrecise = m_context.encoding() == BoogieContext::BV;
		if (bitPrecise) {
			unsigned bits = ASTBoogieUtils::getBits(type);
			return smack::Expr::lit("0", bits);
		} else {
			return smack::Expr::lit((long) 0);
		}
		break;
	}
	case Type::Category::Bool:
		// False
		return smack::Expr::lit(false);
		break;
	default:
		// For unhandled, just return null
		break;
	}

	return nullptr;
}

void ASTBoogieConverter::getVariablesOfType(TypePointer _type, ASTNode const& _scope, std::vector<std::string>& output) {
	std::string target = _type->toString();
	DeclarationContainer const* decl_container = m_context.scopes()[&_scope].get();
	for (; decl_container != nullptr; decl_container = decl_container->enclosingContainer()) {
		for (auto const& decl_pair: decl_container->declarations()) {
			auto const& decl_vector = decl_pair.second;
			for (auto const& decl: decl_vector) {
				if (decl->type()->toString() == target) {
					output.push_back(ASTBoogieUtils::mapDeclName(*decl));
				} else {
					// Structs: go through fields
					if (decl->type()->category() == Type::Category::Struct) {
						auto const& s = dynamic_cast<StructType const&>(*decl->type());
						for (auto const& s_member: s.members(nullptr)) {
							if (s_member.declaration->type()->toString() == target) {
								output.push_back(ASTBoogieUtils::mapDeclName(*s_member.declaration));
							}
						}
					}
					// Magic
					if (decl->type()->category() == Type::Category::Magic) {
						auto const& m = dynamic_cast<MagicType const&>(*decl->type());
						auto const& m_members = m.members(nullptr);
						for (auto const& m_member: m_members) {
							if (m_member.type->toString() == target) {
								// Only sender for now. TODO: Better handling of magic variables
								// and names
								if (m_member.name == ASTBoogieUtils::SOLIDITY_SENDER) {
									output.push_back(ASTBoogieUtils::BOOGIE_MSG_SENDER);
								}
							}
						}
					}
				}
			}
		}
	}
}

bool ASTBoogieConverter::defaultValueAssignment(VariableDeclaration const& _decl, ASTNode const& _scope, std::list<smack::Stmt const*>& output) {

	bool ok = false;

	std::string id = ASTBoogieUtils::mapDeclName(_decl);
	TypePointer type = _decl.type();

	// Default value for the given type
	const smack::Expr* value = defaultValue(type);

	// If there just assign
	if (value) {
		const smack::Stmt* valueAssign = smack::Stmt::assign(smack::Expr::id(id), smack::Expr::upd(
				smack::Expr::id(id),
				smack::Expr::id(ASTBoogieUtils::BOOGIE_THIS), value));
		output.push_back(valueAssign);
		ok = true;
	} else {
		// Otherwise, it's probably a complex type
		switch (type->category()) {
		case Type::Category::Mapping: {

			// Type of the index and element
			TypePointer key_type = dynamic_cast<MappingType const&>(*type).keyType();
			TypePointer element_type = dynamic_cast<MappingType const&>(*type).valueType();
			// Default value for elements
			value = defaultValue(element_type);
			// Get all ids to initialize
			std::vector<std::string> index_ids;
			getVariablesOfType(key_type, _scope, index_ids);
			// Initialize all instantiations to default value
			for (auto index_id: index_ids) {
				// a[this][i] = v
				// a = update(a, this
				//        update(sel(a, this), i, v)
				//     )
				auto map_id = smack::Expr::id(id);
				auto this_i = smack::Expr::id(ASTBoogieUtils::BOOGIE_THIS);
				auto index_i = smack::Expr::id(index_id);
				auto valueAssign = smack::Stmt::assign(map_id,
				        smack::Expr::upd(map_id, this_i,
				        		smack::Expr::upd(smack::Expr::sel(map_id, this_i), index_i, value)));
				output.push_back(valueAssign);
			}
			// Initialize the sum, if there, to default value
			if (m_context.currentSumDecls()[&_decl]) {
				const smack::Expr* sum = smack::Expr::id(id + ASTBoogieUtils::BOOGIE_SUM);
				const smack::Stmt* sum_default = smack::Stmt::assign(sum,
						smack::Expr::upd(sum, smack::Expr::id(ASTBoogieUtils::BOOGIE_THIS), value));
				output.push_back(sum_default);
			}
			ok = true;
			break;
		}
		default:
			// Return null
			break;
		}
	}

	return ok;
}

void ASTBoogieConverter::createDefaultConstructor(ContractDefinition const& _node)
{
	addGlobalComment("");
	addGlobalComment("Default constructor");

	// Input parameters
	list<smack::Binding> params {
		{ASTBoogieUtils::BOOGIE_THIS, ASTBoogieUtils::BOOGIE_ADDRESS_TYPE}, // this
		{ASTBoogieUtils::BOOGIE_MSG_SENDER, ASTBoogieUtils::BOOGIE_ADDRESS_TYPE}, // msg.sender
		{ASTBoogieUtils::BOOGIE_MSG_VALUE, m_context.isBvEncoding() ? "bv256" : "int"} // msg.value
	};

	smack::Block* block = smack::Block::block();
	// State variable initializers should go to the beginning of the constructor
	for (auto i : m_stateVarInitializers) { block->addStmt(i); }
	m_stateVarInitializers.clear(); // Clear list so that we know that initializers have been included
	// Assign uninitialized state variables to default values
	for (auto declNode : m_stateVarsToInitialize) {
		std::list<smack::Stmt const*> stmts;
		bool ok = defaultValueAssignment(*declNode, _node, stmts);
		if (!ok) {
			m_context.reportWarning(&_node, "Boogie: Unhandled default value, constructor verification might fail.");
		}
		for (auto stmt: stmts) {
			block->addStmt(stmt);
		}
	}
	m_stateVarsToInitialize.clear();

	string funcName = ASTBoogieUtils::BOOGIE_CONSTRUCTOR + "#" + toString(_node.id());

	// Create the procedure
	auto procDecl = smack::Decl::procedure(funcName, params, {}, {}, {block});
	for (auto invar : m_context.currentContractInvars())
	{
		procDecl->getEnsures().push_back(smack::Specification::spec(invar.expr,
				ASTBoogieUtils::createAttrs(_node.location(), "State variable initializers might violate invariant '" + invar.exprStr + "'.", *m_context.currentScanner())));
	}
	procDecl->addAttrs(ASTBoogieUtils::createAttrs(_node.location(), "Default constructor", *m_context.currentScanner()));
	m_context.program().getDeclarations().push_back(procDecl);
}

list<BoogieContext::DocTagExpr> ASTBoogieConverter::getExprsFromDocTags(ASTNode const& _node, DocumentedAnnotation const& _annot, ASTNode const* _scope, string _tag)
{
	list<BoogieContext::DocTagExpr> exprs;

	// We use a different error reporter for the type checker to ignore
	// error messages related to our special functions like the __verifier_sum
	ErrorList typeCheckerErrList;
	ErrorReporter typeCheckerErrReporter(typeCheckerErrList);
	TypeChecker typeChecker(m_context.evmVersion(), typeCheckerErrReporter, m_currentContract);

	for (auto docTag : _annot.docTags)
	{
		if (docTag.first == "notice" && boost::starts_with(docTag.second.content, _tag)) // Find expressions with the given tag
		{
			if (_tag == DOCTAG_CONTRACT_INVARS_INCLUDE) // Special tag to include contract invariants
			{
				// TODO: warning when currentinvars is empty
				for (auto invar : m_context.currentContractInvars()) { exprs.push_back(invar); }
			}
			else // Normal tags just parse the expression afterwards
			{
				// We temporarily replace the error reporter in the context, because the locations
				// are pointing to positions in the docstring
				ErrorList exprErrList;
				ErrorReporter exprErrReporter(exprErrList);
				ErrorReporter* originalErrReporter = m_context.errorReporter();
				m_context.errorReporter() = &exprErrReporter;

				// Parse
				string exprStr = docTag.second.content.substr(_tag.length() + 1);
				ASTPointer<Expression> expr = Parser(*m_context.errorReporter())
						.parseExpression(shared_ptr<Scanner>(new Scanner((CharStream)exprStr, "DocString")));
				// Resolve references, using the given scope
				m_context.scopes()[&*expr] = m_context.scopes()[_scope];
				NameAndTypeResolver resolver(m_context.globalDecls(), m_context.scopes(), *m_context.errorReporter());
				resolver.resolveNamesAndTypes(*expr);
				// Do type checking
				typeChecker.checkTypeRequirements(*expr);
				// Convert expression to Boogie representation
				auto result = ASTBoogieExpressionConverter(m_context).convert(*expr);

				// Print errors relating to the expression string
				Scanner sc((CharStream)exprStr, "");
				SourceReferenceFormatter formatter(cerr, [&](string const&) -> solidity::Scanner const& { return sc; });
				for (auto const& error: m_context.errorReporter()->errors())
				{
					formatter.printExceptionInformation(*error,
							(error->type() == Error::Type::Warning) ? "Warning" : "Boogie error");
				}

				// Restore error reporter
				m_context.errorReporter() = originalErrReporter;
				// Add a single error in the original reporter if there were errors
				if (!Error::containsOnlyWarnings(exprErrList))
				{
					m_context.reportError(&_node, "Error while processing annotation for node");
				}

				if (!result.newStatements.empty()) // Make sure that there are no side effects
				{
					m_context.reportError(&_node, "Annotation expression introduces intermediate statements");
				}
				if (!result.newDecls.empty()) // Make sure that there are no side effects
				{
					m_context.reportError(&_node, "Annotation expression introduces intermediate declarations");
				}
				exprs.push_back(BoogieContext::DocTagExpr(result.expr, exprStr, result.tccs));
			}
		}
	}

	return exprs;
}

ASTBoogieConverter::ASTBoogieConverter(BoogieContext& context) :
				m_context(context),
				m_currentRet(nullptr),
				m_nextReturnLabelId(0)
{
	// Initialize global declarations
	addGlobalComment("Global declarations and definitions related to the address type");
	// address type
	m_context.program().getDeclarations().push_back(smack::Decl::typee(ASTBoogieUtils::BOOGIE_ADDRESS_TYPE));
	m_context.program().getDeclarations().push_back(smack::Decl::constant(ASTBoogieUtils::BOOGIE_ZERO_ADDRESS, ASTBoogieUtils::BOOGIE_ADDRESS_TYPE, true));
	// address.balance
	m_context.program().getDeclarations().push_back(smack::Decl::variable(ASTBoogieUtils::BOOGIE_BALANCE,
			"[" + ASTBoogieUtils::BOOGIE_ADDRESS_TYPE + "]" + (m_context.isBvEncoding() ? "bv256" : "int")));
	// Uninterpreted type for strings
	m_context.program().getDeclarations().push_back(smack::Decl::typee(ASTBoogieUtils::BOOGIE_STRING_TYPE));
	// now
	m_context.program().getDeclarations().push_back(smack::Decl::variable(ASTBoogieUtils::BOOGIE_NOW, m_context.isBvEncoding() ? "bv256" : "int"));
}

// ---------------------------------------------------------------------------
//         Visitor methods for top-level nodes and declarations
// ---------------------------------------------------------------------------

bool ASTBoogieConverter::visit(SourceUnit const& _node)
{
	rememberScope(_node);

	// Boogie programs are flat, source units do not appear explicitly
	addGlobalComment("");
	addGlobalComment("------- Source: " + _node.annotation().path + " -------");
	return true; // Simply apply visitor recursively
}

bool ASTBoogieConverter::visit(PragmaDirective const& _node)
{
	rememberScope(_node);

	// Pragmas are only included as comments
	addGlobalComment("Pragma: " + boost::algorithm::join(_node.literals(), ""));
	return false;
}

bool ASTBoogieConverter::visit(ImportDirective const& _node)
{
	rememberScope(_node);

//	BOOST_THROW_EXCEPTION(InternalCompilerError() << errinfo_comment("Unhandled node: ImportDirective") << errinfo_sourceLocation(_node.location()));
	return false;
}

bool ASTBoogieConverter::visit(ContractDefinition const& _node)
{
	rememberScope(_node);

	m_currentContract = &_node;
	// Boogie programs are flat, contracts do not appear explicitly
	addGlobalComment("");
	addGlobalComment("------- Contract: " + _node.name() + " -------");

	// Process contract invariants
	m_context.currentContractInvars().clear();
	m_context.currentSumDecls().clear();

	for (auto invar : getExprsFromDocTags(_node, _node.annotation(), &_node, DOCTAG_CONTRACT_INVAR))
	{
		addGlobalComment("Contract invariant: " + invar.exprStr);
		m_context.currentContractInvars().push_back(invar);
	}

	// Add new shadow variables for sum
	for (auto sumDecl : m_context.currentSumDecls())
	{
		addGlobalComment("Shadow variable for sum over '" + sumDecl.first->name() + "'");
		m_context.program().getDeclarations().push_back(
					smack::Decl::variable(ASTBoogieUtils::mapDeclName(*sumDecl.first) + ASTBoogieUtils::BOOGIE_SUM,
					"[" + ASTBoogieUtils::BOOGIE_ADDRESS_TYPE + "]" +
					ASTBoogieUtils::mapType(sumDecl.second, *sumDecl.first, m_context)));
	}

	// Process state variables first (to get initializer expressions)
	m_stateVarInitializers.clear();
	m_stateVarsToInitialize.clear();
	for (auto sn : _node.subNodes())
	{
		if (dynamic_cast<VariableDeclaration const*>(&*sn)) { sn->accept(*this); }
	}

	// Process other elements
	for (auto sn : _node.subNodes())
	{
		if (!dynamic_cast<VariableDeclaration const*>(&*sn)) { sn->accept(*this); }
	}

	// If there are still initializers left, there was no constructor, so we create one
	if (!m_stateVarInitializers.empty()) { createDefaultConstructor(_node); }

	return false;
}

bool ASTBoogieConverter::visit(InheritanceSpecifier const& _node)
{
	rememberScope(_node);

	// TODO: calling constructor of superclass?
	// Boogie programs are flat, inheritance does not appear explicitly
	addGlobalComment("Inherits from: " + boost::algorithm::join(_node.name().namePath(), "#"));
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
	addGlobalComment("Using " + libraryName + " for " + typeName);
	return false;
}

bool ASTBoogieConverter::visit(StructDefinition const& _node)
{
	rememberScope(_node);

	m_context.reportError(&_node, "Struct definitions are not supported");
	return false;
}

bool ASTBoogieConverter::visit(EnumDefinition const& _node)
{
	rememberScope(_node);

	m_context.reportError(&_node, "Enum definitions are not supported");
	return false;
}

bool ASTBoogieConverter::visit(EnumValue const& _node)
{
	rememberScope(_node);

	BOOST_THROW_EXCEPTION(InternalCompilerError() << errinfo_comment("Unhandled node: EnumValue") << errinfo_sourceLocation(_node.location()));
	return false;
}

bool ASTBoogieConverter::visit(ParameterList const& _node)
{
	rememberScope(_node);

	BOOST_THROW_EXCEPTION(InternalCompilerError() << errinfo_comment("Unhandled node: ParameterList") << errinfo_sourceLocation(_node.location()));
	return false;
}

bool ASTBoogieConverter::visit(FunctionDefinition const& _node)
{
	rememberScope(_node);

	// Solidity functions are mapped to Boogie procedures
	m_currentFunc = &_node;

	addGlobalComment("");
	string funcType = _node.visibility() == Declaration::Visibility::External ? "" : " : " + _node.type()->toString();
	addGlobalComment("Function: " + _node.name() + funcType);

	// Input parameters
	list<smack::Binding> params {
		// Globally available stuff
		{ASTBoogieUtils::BOOGIE_THIS, ASTBoogieUtils::BOOGIE_ADDRESS_TYPE}, // this
		{ASTBoogieUtils::BOOGIE_MSG_SENDER, ASTBoogieUtils::BOOGIE_ADDRESS_TYPE}, // msg.sender
		{ASTBoogieUtils::BOOGIE_MSG_VALUE, m_context.isBvEncoding() ? "bv256" : "int"} // msg.value
	};
	// Add original parameters of the function
	for (auto par : _node.parameters())
	{
		params.push_back({ASTBoogieUtils::mapDeclName(*par), ASTBoogieUtils::mapType(par->type(), *par, m_context)});
		if (par->type()->category() == Type::Category::Array) // Array length
		{
			params.push_back({ASTBoogieUtils::mapDeclName(*par) + ASTBoogieUtils::BOOGIE_LENGTH, m_context.isBvEncoding() ? "bv256" : "int"});
		}
	}

	// Return values
	list<smack::Binding> rets;
	for (auto ret : _node.returnParameters())
	{
		if (ret->type()->category() == Type::Category::Array)
		{
			m_context.reportError(&_node, "Arrays are not supported as return values");
			return false;
		}
		rets.push_back({ASTBoogieUtils::mapDeclName(*ret), ASTBoogieUtils::mapType(ret->type(), *ret, m_context)});
	}

	// Boogie treats return as an assignment to the return variable(s)
	if (_node.returnParameters().empty())
	{
		m_currentRet = nullptr;
	}
	else if (_node.returnParameters().size() == 1)
	{
		m_currentRet = smack::Expr::id(rets.begin()->first);
	}
	else
	{
		m_context.reportError(&_node, "Multiple return values are not supported");
		return false;
	}

	// Convert function body, collect result
	m_localDecls.clear();
	// Create new empty block
	m_currentBlocks.push(smack::Block::block());
	// State variable initializers should go to the beginning of the constructor
	if (_node.isConstructor())
	{
		for (auto i : m_stateVarInitializers) m_currentBlocks.top()->addStmt(i);
		m_stateVarInitializers.clear(); // Clear list so that we know that initializers have been included
		// Assign uninitialized state variables to default values
		for (auto declNode : m_stateVarsToInitialize) {
			std::list<smack::Stmt const*> stmts;
			bool ok = defaultValueAssignment(*declNode, _node, stmts);
			if (!ok) {
				m_context.reportWarning(&_node, "Boogie: Unhandled default value, constructor verification might fail.");
			}
			for (auto stmt : stmts) {
				m_currentBlocks.top()->addStmt(stmt);
			}
		}
		m_stateVarsToInitialize.clear();
	}
	// Payable functions should handle msg.value
	if (_node.isPayable())
	{
		// balance[this] += msg.value
		auto addResult = ASTBoogieUtils::encodeArithBinaryOp(m_context, nullptr, Token::Value::Add,
				smack::Expr::sel(ASTBoogieUtils::BOOGIE_BALANCE, ASTBoogieUtils::BOOGIE_THIS),
				smack::Expr::id(ASTBoogieUtils::BOOGIE_MSG_VALUE), 256, false);
		m_currentBlocks.top()->addStmt(smack::Stmt::assign(
					smack::Expr::id(ASTBoogieUtils::BOOGIE_BALANCE),
					smack::Expr::upd(
							smack::Expr::id(ASTBoogieUtils::BOOGIE_BALANCE),
							smack::Expr::id(ASTBoogieUtils::BOOGIE_THIS),
							addResult.first)));
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
			m_currentBlocks.top()->addStmt(smack::Stmt::label(retLabel));
		}
	}
	else
	{
		m_currentModifier = 0;
		auto modifier = _node.modifiers()[m_currentModifier];
		auto modifierDecl = dynamic_cast<ModifierDefinition const*>(modifier->name()->annotation().referencedDeclaration);

		if (modifierDecl)
		{
			string retLabel = "$return" + to_string(m_nextReturnLabelId);
			++m_nextReturnLabelId;
			m_currentReturnLabel = retLabel;
			m_currentBlocks.top()->addStmt(smack::Stmt::comment("Inlined modifier " + modifierDecl->name() + " starts here"));

			// Introduce and assign local variables for modifier arguments
			if (modifier->arguments())
			{
				for (unsigned long i = 0; i < modifier->arguments()->size(); ++i)
				{
					smack::Decl* modifierParam = smack::Decl::variable(ASTBoogieUtils::mapDeclName(*(modifierDecl->parameters()[i])),
							ASTBoogieUtils::mapType(modifierDecl->parameters()[i]->annotation().type, *(modifierDecl->parameters()[i]), m_context));
					m_localDecls.push_back(modifierParam);
					smack::Expr const* modifierArg = convertExpression(*modifier->arguments()->at(i));
					m_currentBlocks.top()->addStmt(smack::Stmt::assign(smack::Expr::id(modifierParam->getName()), modifierArg));
				}
			}

			modifierDecl->body().accept(*this);
			m_currentBlocks.top()->addStmt(smack::Stmt::label(retLabel));
			m_currentBlocks.top()->addStmt(smack::Stmt::comment("Inlined modifier " + modifierDecl->name() + " ends here"));
		}
		else
		{
			m_context.reportError(&*modifier, "Unsupported modifier invocation");
		}
	}

	list<smack::Block*> blocks;
	if (!m_currentBlocks.top()->getStatements().empty()) { blocks.push_back(m_currentBlocks.top()); }
	m_currentBlocks.pop();
	if (!m_currentBlocks.empty())
	{
		BOOST_THROW_EXCEPTION(InternalCompilerError() <<
				errinfo_comment("Non-empty stack of blocks at the end of function."));
	}

	// Get the name of the function
	string funcName = _node.isConstructor() ?
			ASTBoogieUtils::BOOGIE_CONSTRUCTOR + "#" + toString(_node.id()) : // TODO: we should use the ID of the contract (the default constructor can only use that)
			ASTBoogieUtils::mapDeclName(_node);

	// Create the procedure
	auto procDecl = smack::Decl::procedure(funcName, params, rets, m_localDecls, blocks);

	if (_node.isPublic()) // Public functions: add invariants as pre/postconditions
	{
		for (auto invar : m_context.currentContractInvars())
		{
			if (!_node.isConstructor())
			{
				procDecl->getRequires().push_back(smack::Specification::spec(invar.expr,
					ASTBoogieUtils::createAttrs(_node.location(), "Invariant '" + invar.exprStr + "' might not hold when entering function.", *m_context.currentScanner())));
			}
			procDecl->getEnsures().push_back(smack::Specification::spec(invar.expr,
					ASTBoogieUtils::createAttrs(_node.location(), "Invariant '" + invar.exprStr + "' might not hold at end of function.", *m_context.currentScanner())));

			for (auto tcc : invar.tccs)
			{
				procDecl->getRequires().push_back(smack::Specification::spec(tcc,
					ASTBoogieUtils::createAttrs(_node.location(), "Variables in invariant '" + invar.exprStr + "' might be out of range when entering function.", *m_context.currentScanner())));
				procDecl->getEnsures().push_back(smack::Specification::spec(tcc,
					ASTBoogieUtils::createAttrs(_node.location(), "Variables in invariant '" + invar.exprStr + "' might be out of range at end of function.", *m_context.currentScanner())));
			}
		}
	}
	else // Private functions: inline
	{
		procDecl->addAttr(smack::Attr::attr("inline", 1));

		// Add contract invariants only if explicitly requested
		for (auto cInv : getExprsFromDocTags(_node, _node.annotation(), &_node, DOCTAG_CONTRACT_INVARS_INCLUDE))
		{
			procDecl->getRequires().push_back(smack::Specification::spec(cInv.expr,
					ASTBoogieUtils::createAttrs(_node.location(), "Invariant '" + cInv.exprStr + "' might not hold when entering function.", *m_context.currentScanner())));
			procDecl->getEnsures().push_back(smack::Specification::spec(cInv.expr,
					ASTBoogieUtils::createAttrs(_node.location(), "Invariant '" + cInv.exprStr + "' might not hold at end of function.", *m_context.currentScanner())));

			for (auto tcc : cInv.tccs)
			{
				procDecl->getRequires().push_back(smack::Specification::spec(tcc,
					ASTBoogieUtils::createAttrs(_node.location(), "Variables in invariant '" + cInv.exprStr + "' might be out of range when entering function.", *m_context.currentScanner())));
				procDecl->getEnsures().push_back(smack::Specification::spec(tcc,
					ASTBoogieUtils::createAttrs(_node.location(), "Variables in invariant '" + cInv.exprStr + "' might be out of range at end of function.", *m_context.currentScanner())));
			}
		}
	}

	// Add other pre/postconditions
	for (auto pre : getExprsFromDocTags(_node, _node.annotation(), &_node, DOCTAG_PRECOND))
	{
		procDecl->getRequires().push_back(smack::Specification::spec(pre.expr,
							ASTBoogieUtils::createAttrs(_node.location(), "Precondition '" + pre.exprStr + "' might not hold when entering function.", *m_context.currentScanner())));
		for (auto tcc : pre.tccs) { procDecl->getRequires().push_back(smack::Specification::spec(tcc,
				ASTBoogieUtils::createAttrs(_node.location(), "Variables in precondition '" + pre.exprStr + "' might be out of range when entering function.", *m_context.currentScanner()))); }
	}
	for (auto post : getExprsFromDocTags(_node, _node.annotation(), &_node, DOCTAG_POSTCOND))
	{
		procDecl->getEnsures().push_back(smack::Specification::spec(post.expr,
							ASTBoogieUtils::createAttrs(_node.location(), "Postcondition '" + post.exprStr + "' might not hold at end of function.", *m_context.currentScanner())));
		for (auto tcc : post.tccs) { procDecl->getEnsures().push_back(smack::Specification::spec(tcc,
						ASTBoogieUtils::createAttrs(_node.location(), "Variables in postcondition '" + post.exprStr + "' might be out of range at end of function.", *m_context.currentScanner()))); }
	}
	// TODO: check that no new sum variables were introduced

	procDecl->addAttrs(ASTBoogieUtils::createAttrs(_node.location(), _node.name(), *m_context.currentScanner()));
	m_context.program().getDeclarations().push_back(procDecl);
	return false;
}

bool ASTBoogieConverter::visit(VariableDeclaration const& _node)
{
	rememberScope(_node);

	// Non-state variables should be handled in the VariableDeclarationStatement
	if (!_node.isStateVariable())
	{
		BOOST_THROW_EXCEPTION(InternalCompilerError() <<
				errinfo_comment("Non-state variable appearing in VariableDeclaration") <<
				errinfo_sourceLocation(_node.location()));
	}
	if (_node.value())
	{
		// Initialization is saved for the constructor
		// A new block is introduced to collect side effects of the initializer expression
		m_currentBlocks.push(smack::Block::block());
		smack::Expr const* initExpr = convertExpression(*_node.value());
		if (m_context.isBvEncoding() && ASTBoogieUtils::isBitPreciseType(_node.annotation().type))
		{
			initExpr = ASTBoogieUtils::checkImplicitBvConversion(initExpr, _node.value()->annotation().type, _node.annotation().type, m_context);
		}
		for (auto stmt : *m_currentBlocks.top()) { m_stateVarInitializers.push_back(stmt); }
		m_currentBlocks.pop();
		m_stateVarInitializers.push_back(smack::Stmt::assign(smack::Expr::id(ASTBoogieUtils::mapDeclName(_node)),
				smack::Expr::upd(
						smack::Expr::id(ASTBoogieUtils::mapDeclName(_node)),
						smack::Expr::id(ASTBoogieUtils::BOOGIE_THIS),
						initExpr)));
	} else {
		// Use implicit default value (will generate later, once we are in scope of constructor)
		m_stateVarsToInitialize.push_back(&_node);
	}

	addGlobalComment("");
	addGlobalComment("State variable: " + _node.name() + " : " + _node.type()->toString());
	// State variables are represented as maps from address to their type
	auto varDecl = smack::Decl::variable(ASTBoogieUtils::mapDeclName(_node),
			"[" + ASTBoogieUtils::BOOGIE_ADDRESS_TYPE + "]" + ASTBoogieUtils::mapType(_node.type(), _node, m_context));
	varDecl->addAttrs(ASTBoogieUtils::createAttrs(_node.location(), _node.name(), *m_context.currentScanner()));
	m_context.program().getDeclarations().push_back(varDecl);

	// Arrays require an extra variable for their length
	if (_node.type()->category() == Type::Category::Array)
	{
		m_context.program().getDeclarations().push_back(
				smack::Decl::variable(ASTBoogieUtils::mapDeclName(_node) + ASTBoogieUtils::BOOGIE_LENGTH,
				"[" + ASTBoogieUtils::BOOGIE_ADDRESS_TYPE + "]" + (m_context.isBvEncoding() ? "bv256" : "int")));
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

	BOOST_THROW_EXCEPTION(InternalCompilerError() << errinfo_comment("Unhandled node: ModifierInvocation") << errinfo_sourceLocation(_node.location()));
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

	BOOST_THROW_EXCEPTION(InternalCompilerError() << errinfo_comment("Unhandled node: ElementaryTypeName") << errinfo_sourceLocation(_node.location()));
	return false;
}

bool ASTBoogieConverter::visit(UserDefinedTypeName const& _node)
{
	rememberScope(_node);

	BOOST_THROW_EXCEPTION(InternalCompilerError() << errinfo_comment("Unhandled node: UserDefinedTypeName") << errinfo_sourceLocation(_node.location()));
	return false;
}

bool ASTBoogieConverter::visit(FunctionTypeName const& _node)
{
	rememberScope(_node);

	BOOST_THROW_EXCEPTION(InternalCompilerError() << errinfo_comment("Unhandled node: FunctionTypeName") << errinfo_sourceLocation(_node.location()));
	return false;
}

bool ASTBoogieConverter::visit(Mapping const& _node)
{
	rememberScope(_node);

	BOOST_THROW_EXCEPTION(InternalCompilerError() << errinfo_comment("Unhandled node: Mapping") << errinfo_sourceLocation(_node.location()));
	return false;
}

bool ASTBoogieConverter::visit(ArrayTypeName const& _node)
{
	rememberScope(_node);

	BOOST_THROW_EXCEPTION(InternalCompilerError() << errinfo_comment("Unhandled node: ArrayTypeName") << errinfo_sourceLocation(_node.location()));
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

	// We go one level deeper in the modifier list
	m_currentModifier++;

	if (m_currentModifier < m_currentFunc->modifiers().size()) // We still have modifiers
	{
		auto modifier = m_currentFunc->modifiers()[m_currentModifier];
		auto modifierDecl = dynamic_cast<ModifierDefinition const*>(modifier->name()->annotation().referencedDeclaration);

		if (modifierDecl)
		{
			// Duplicated modifiers currently do not work, because they will introduce
			// local variables for their parameters with the same name
			for (unsigned long i = 0; i < m_currentModifier; ++i)
			{
				if (m_currentFunc->modifiers()[i]->name()->annotation().referencedDeclaration->id() == modifierDecl->id()) {
					m_context.reportError(m_currentFunc, "Duplicated modifiers are not supported");
				}
			}

			string oldReturnLabel = m_currentReturnLabel;
			m_currentReturnLabel = "$return" + to_string(m_nextReturnLabelId);
			++m_nextReturnLabelId;
			m_currentBlocks.top()->addStmt(smack::Stmt::comment("Inlined modifier " + modifierDecl->name() + " starts here"));

			// Introduce and assign local variables for modifier arguments
			if (modifier->arguments())
			{
				for (unsigned long i = 0; i < modifier->arguments()->size(); ++i)
				{
					smack::Decl* modifierParam = smack::Decl::variable(ASTBoogieUtils::mapDeclName(*(modifierDecl->parameters()[i])),
							ASTBoogieUtils::mapType(modifierDecl->parameters()[i]->annotation().type, *(modifierDecl->parameters()[i]), m_context));
					m_localDecls.push_back(modifierParam);
					smack::Expr const* modifierArg = convertExpression(*modifier->arguments()->at(i));
					m_currentBlocks.top()->addStmt(smack::Stmt::assign(smack::Expr::id(modifierParam->getName()), modifierArg));
				}
			}
			modifierDecl->body().accept(*this);
			m_currentBlocks.top()->addStmt(smack::Stmt::label(m_currentReturnLabel));
			m_currentBlocks.top()->addStmt(smack::Stmt::comment("Inlined modifier " + modifierDecl->name() + " ends here"));
			m_currentReturnLabel = oldReturnLabel;
		}
		else
		{
			m_context.reportError(&*modifier, "Unsupported modifier invocation");
		}
	}
	else if (m_currentFunc->isImplemented()) // We reached the function
	{
		string oldReturnLabel = m_currentReturnLabel;
		m_currentReturnLabel = "$return" + to_string(m_nextReturnLabelId);
		++m_nextReturnLabelId;
		m_currentBlocks.top()->addStmt(smack::Stmt::comment("Function body starts here"));
		m_currentFunc->body().accept(*this);
		m_currentBlocks.top()->addStmt(smack::Stmt::label(m_currentReturnLabel));
		m_currentBlocks.top()->addStmt(smack::Stmt::comment("Function body ends here"));
		m_currentReturnLabel = oldReturnLabel;
	}

	m_currentModifier--; // Go back one level in the modifier list

	return false;
}

bool ASTBoogieConverter::visit(IfStatement const& _node)
{
	rememberScope(_node);

	// Get condition recursively
	const smack::Expr* cond = convertExpression(_node.condition());

	// Get true branch recursively
	m_currentBlocks.push(smack::Block::block());
	_node.trueStatement().accept(*this);
	const smack::Block* thenBlock = m_currentBlocks.top();
	m_currentBlocks.pop();

	// Get false branch recursively (might not exist)
	const smack::Block* elseBlock = nullptr;
	if (_node.falseStatement())
	{
		m_currentBlocks.push(smack::Block::block());
		_node.falseStatement()->accept(*this);
		elseBlock = m_currentBlocks.top();
		m_currentBlocks.pop();
	}

	m_currentBlocks.top()->addStmt(smack::Stmt::ifelse(cond, thenBlock, elseBlock));
	return false;
}

bool ASTBoogieConverter::visit(WhileStatement const& _node)
{
	rememberScope(_node);

	// Get condition recursively
	const smack::Expr* cond = convertExpression(_node.condition());

	// Get if branch recursively
	m_currentBlocks.push(smack::Block::block());
	_node.body().accept(*this);
	const smack::Block* body = m_currentBlocks.top();
	m_currentBlocks.pop();

	std::list<smack::Specification const*> invars;
	for (auto invar : getExprsFromDocTags(_node, _node.annotation(), scope(), DOCTAG_LOOP_INVAR))
	{
		for (auto tcc : invar.tccs) { invars.push_back(smack::Specification::spec(tcc,
												ASTBoogieUtils::createAttrs(_node.location(), "variables in range for '" + invar.exprStr + "'", *m_context.currentScanner()))); }
		invars.push_back(smack::Specification::spec(invar.expr, ASTBoogieUtils::createAttrs(_node.location(), invar.exprStr, *m_context.currentScanner())));
	}
	for (auto invar : getExprsFromDocTags(_node, _node.annotation(), scope(), DOCTAG_CONTRACT_INVARS_INCLUDE))
	{
		for (auto tcc : invar.tccs) { invars.push_back(smack::Specification::spec(tcc,
												ASTBoogieUtils::createAttrs(_node.location(), "variables in range for '" + invar.exprStr + "'", *m_context.currentScanner()))); }
		invars.push_back(smack::Specification::spec(invar.expr, ASTBoogieUtils::createAttrs(_node.location(), invar.exprStr, *m_context.currentScanner())));
	}
	// TODO: check that invariants did not introduce new sum variables

	m_currentBlocks.top()->addStmt(smack::Stmt::while_(cond, body, invars));

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

	// Get initialization recursively (adds statement to current block)
	m_currentBlocks.top()->addStmt(smack::Stmt::comment("The following while loop was mapped from a for loop"));
	if (_node.initializationExpression()) { _node.initializationExpression()->accept(*this); }

	// Get condition recursively
	const smack::Expr* cond = _node.condition() ? convertExpression(*_node.condition()) : nullptr;

	// Get body recursively
	m_currentBlocks.push(smack::Block::block());
	_node.body().accept(*this);
	// Include loop expression at the end of body
	if (_node.loopExpression())
	{
		_node.loopExpression()->accept(*this); // Adds statements to current block
	}
	const smack::Block* body = m_currentBlocks.top();
	m_currentBlocks.pop();

	std::list<smack::Specification const*> invars;
	for (auto invar : getExprsFromDocTags(_node, _node.annotation(), &_node, DOCTAG_LOOP_INVAR))
	{
		for (auto tcc : invar.tccs) { invars.push_back(smack::Specification::spec(tcc,
												ASTBoogieUtils::createAttrs(_node.location(), "variables in range for '" + invar.exprStr + "'", *m_context.currentScanner()))); }
		invars.push_back(smack::Specification::spec(invar.expr, ASTBoogieUtils::createAttrs(_node.location(), invar.exprStr, *m_context.currentScanner())));
	}
	for (auto invar : getExprsFromDocTags(_node, _node.annotation(), &_node, DOCTAG_CONTRACT_INVARS_INCLUDE))
	{
		for (auto tcc : invar.tccs) { invars.push_back(smack::Specification::spec(tcc,
												ASTBoogieUtils::createAttrs(_node.location(), "variables in range for '" + invar.exprStr + "'", *m_context.currentScanner()))); }
		invars.push_back(smack::Specification::spec(invar.expr, ASTBoogieUtils::createAttrs(_node.location(), invar.exprStr, *m_context.currentScanner())));
	}
	// TODO: check that invariants did not introduce new sum variables

	m_currentBlocks.top()->addStmt(smack::Stmt::while_(cond, body, invars));

	return false;
}

bool ASTBoogieConverter::visit(Continue const& _node)
{
	rememberScope(_node);

	// TODO: Boogie does not support continue, this must be mapped manually
	// using labels and gotos
	m_context.reportError(&_node, "Continue statement is not supported");
	return false;
}

bool ASTBoogieConverter::visit(Break const& _node)
{
	rememberScope(_node);

	m_currentBlocks.top()->addStmt(smack::Stmt::break_());
	return false;
}

bool ASTBoogieConverter::visit(Return const& _node)
{
	rememberScope(_node);

	if (_node.expression() != nullptr)
	{
		// Get rhs recursively
		const smack::Expr* rhs = convertExpression(*_node.expression());

		if (m_context.isBvEncoding())
		{
			// We already throw an error elsewhere if there are multiple return values
			auto returnType = m_currentFunc->returnParameters()[0]->annotation().type;
			rhs = ASTBoogieUtils::checkImplicitBvConversion(rhs, _node.expression()->annotation().type,
					returnType, m_context);
		}

		// LHS of assignment should already be known (set by the enclosing FunctionDefinition)
		const smack::Expr* lhs = m_currentRet;

		// First create an assignment, and then an empty return
		m_currentBlocks.top()->addStmt(smack::Stmt::assign(lhs, rhs));
	}
	m_currentBlocks.top()->addStmt(smack::Stmt::goto_({m_currentReturnLabel}));
	return false;
}

bool ASTBoogieConverter::visit(Throw const& _node)
{
	rememberScope(_node);

	m_currentBlocks.top()->addStmt(smack::Stmt::assume(smack::Expr::lit(false)));
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

	for (auto decl : _node.declarations())
	{
		// Decl can be null, e.g., var (x,,) = (1,2,3)
		// In this case we just ignore it
		if (decl != nullptr)
		{
			if (decl->isLocalVariable())
			{
				// Boogie requires local variables to be declared at the beginning of the procedure
				auto varDecl = smack::Decl::variable(
						ASTBoogieUtils::mapDeclName(*decl),
						ASTBoogieUtils::mapType(decl->type(), *decl, m_context));
				varDecl->addAttrs(ASTBoogieUtils::createAttrs(decl->location(), decl->name(), *m_context.currentScanner()));
				m_localDecls.push_back(varDecl);
			}
			else
			{
				// Non-local variables should be handled elsewhere
				BOOST_THROW_EXCEPTION(InternalCompilerError() <<
						errinfo_comment("Non-local variable appearing in VariableDeclarationStatement") <<
						errinfo_sourceLocation(_node.location()));
			}
		}
	}

	// Convert initial value into an assignment statement
	if (_node.initialValue())
	{
		if (_node.declarations().size() == 1)
		{
			auto decl = *_node.declarations().begin();
			// Get expression recursively
			const smack::Expr* rhs = convertExpression(*_node.initialValue());

			if (m_context.isBvEncoding() && ASTBoogieUtils::isBitPreciseType(decl->annotation().type))
			{
				rhs = ASTBoogieUtils::checkImplicitBvConversion(rhs, _node.initialValue()->annotation().type, decl->annotation().type, m_context);
			}

			m_currentBlocks.top()->addStmt(smack::Stmt::assign(
					smack::Expr::id(ASTBoogieUtils::mapDeclName(**_node.declarations().begin())),
					rhs));
		}
		else
		{
			m_context.reportError(&_node, "Assignment to multiple variables is not supported");
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

	smack::Expr const* expr = convertExpression(_node.expression());
	smack::Decl* tmpVar = smack::Decl::variable("tmpVar" + to_string(_node.id()),
			ASTBoogieUtils::mapType(_node.expression().annotation().type, _node, m_context));
	m_localDecls.push_back(tmpVar);
	m_currentBlocks.top()->addStmt(smack::Stmt::comment("Assignment to temp variable introduced because Boogie does not support stand alone expressions"));
	m_currentBlocks.top()->addStmt(smack::Stmt::assign(smack::Expr::id(tmpVar->getName()), expr));
	return false;
}


bool ASTBoogieConverter::visitNode(ASTNode const& _node)
{
	rememberScope(_node);

	BOOST_THROW_EXCEPTION(InternalCompilerError() << errinfo_comment("Unhandled node (unknown)") << errinfo_sourceLocation(_node.location()));
	return true;
}

}
}
