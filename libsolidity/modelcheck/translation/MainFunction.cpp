/**
 * First-pass visitor for generating Solidity the first part of main function,
 * which consist of the decalaration of contract, globalstate, nextGS
 * and every input parameters of functions in main function.
 * @date 2019
 */

#include <libsolidity/modelcheck/translation/MainFunction.h>

#include <libsolidity/modelcheck/analysis/CallState.h>
#include <libsolidity/modelcheck/analysis/VariableScope.h>
#include <libsolidity/modelcheck/codegen/Details.h>
#include <libsolidity/modelcheck/codegen/Literals.h>
#include <libsolidity/modelcheck/utils/Contract.h>
#include <libsolidity/modelcheck/utils/Function.h>

using namespace std;

namespace dev
{
namespace solidity
{
namespace modelcheck
{

// -------------------------------------------------------------------------- //

MainFunctionGenerator::MainFunctionGenerator(
    list<ContractDefinition const *> const& _model,
    NewCallGraph const& _new_graph,
    CallState const& _statedata,
    TypeConverter const& _converter
): m_model(_model)
 , m_new_graph(_new_graph)
 , m_statedata(_statedata)
 , m_converter(_converter)
{
}

// -------------------------------------------------------------------------- //

void MainFunctionGenerator::record(SourceUnit const& _ast)
{
    auto inset = ASTNode::filteredNodes<ContractDefinition>(_ast.nodes());
    m_contracts.insert(m_contracts.end(), inset.begin(), inset.end());
}

// -------------------------------------------------------------------------- //

void MainFunctionGenerator::print(std::ostream& _stream)
{
    // Determines the contracts to use by the harness.
    vector<ContractDefinition const*> model;
    if (!m_model.empty())
    {
        model.reserve(m_model.size());
        model.insert(model.begin(), m_model.begin(), m_model.end());
    }
    else
    {
        model.reserve(m_contracts.size());
        model.insert(model.begin(), m_contracts.begin(), m_contracts.end());
    }

    // Pre-analyzes contracts for fields, etc.
    list<Actor> actors = analyze_decls(model);

    // Generates function switch.
    auto call_cases = make_shared<CSwitch>(
        get_nd_byte("Select next call"),
        CBlockList{make_require(Literals::ZERO)}
    );
    for (auto actor : actors)
    {
        for (auto const* FUNC : actor.contract->definedFunctions())
        {
            if (FUNC->isConstructor() || !FUNC->isPublic()) continue;
            auto call_body = build_case(*FUNC, actor.fparams, actor.decl);
            call_cases->add_case(actor.fnums[FUNC], move(call_body));
        }
    }

    // Generates fixed-point iteration.
    CBlockList fixpoint;
    fixpoint.push_back(
        make_shared<CFuncCall>("sol_on_transaction", CArgList{})->stmt()
    );
    update_call_state(fixpoint);
    fixpoint.push_back(call_cases);

    // Contract setup and tear-down.
    list<CExprPtr> addresses;
    CBlockList main;
    for (auto fld : m_statedata.order())
    {
        auto const DECL = make_shared<CVarDecl>(fld.tname, fld.name);
        main.push_back(DECL);

        if (fld.field == CallStateUtilities::Field::Block)
        {
            auto const TMP_DECL = make_shared<CVarDecl>(fld.tname, fld.temp);
            main.push_back(TMP_DECL);
            main.push_back(DECL->access("v")->assign(Literals::ZERO)->stmt());
        }
    } 
    update_call_state(main);
    for (auto actor : actors)
    {
        for (auto param_pair : actor.fparams) main.push_back(param_pair.second);
        auto const& DECL = actor.decl;
        auto const& ADDR = DECL->access(ContractUtilities::address_member());
        string const ADDRMSG = "Init address of " + actor.contract->name();
        main.push_back(DECL);
        if (actor.path)
        {
            main.push_back(
                DECL->assign(make_shared<CReference>(actor.path))->stmt()
            );
        }
        else
        {
            main.push_back(init_contract(*actor.contract, DECL));
        }
        main.push_back(ADDR->assign(
            TypeConverter::nd_val_by_simple_type(
                *ContractUtilities::address_type(), ADDRMSG
            )
        )->stmt());
        main.push_back(make_require(make_shared<CBinaryOp>(
            make_shared<CMemberAccess>(ADDR, "v"), "!=", Literals::ZERO
        )));
        for (auto other_addr : addresses)
        {
            main.push_back(make_require(make_shared<CBinaryOp>(
                make_shared<CMemberAccess>(ADDR, "v"),
                "!=", 
                make_shared<CMemberAccess>(other_addr, "v")
            )));
        }
        addresses.push_back(ADDR);
    }
    main.push_back(make_shared<CWhileLoop>(
        make_shared<CBlock>(move(fixpoint)),
        get_nd_byte("Select 0 to terminate"),
        false
    ));

    auto id = make_shared<CVarDecl>("void", "run_model");
    _stream << CFuncDef(id, CParams{}, make_shared<CBlock>(move(main)));
}

// -------------------------------------------------------------------------- //

CStmtPtr MainFunctionGenerator::make_require(CExprPtr _cond)
{
    auto fn = make_shared<CFuncCall>(
        "sol_require", CArgList{_cond, Literals::ZERO
    });
    return fn->stmt();
}

// -------------------------------------------------------------------------- //

MainFunctionGenerator::Actor::Actor(
    TypeConverter const& _converter,
    ContractDefinition const* _contract,
    CExprPtr _path,
    TicketSystem<uint16_t> & _cids,
    TicketSystem<uint16_t> & _fids
): contract(_contract), path(_path)
{
    uint16_t cid = _cids.next();

    decl = make_shared<CVarDecl>(
        _converter.get_type(*_contract),
        "contract_" + to_string(cid),
        _path != nullptr
    );

    for (size_t fidx = 0; fidx < contract->definedFunctions().size(); ++fidx)
    {
        auto const* FUNC = contract->definedFunctions()[fidx];
        if (FUNC->isConstructor() || !FUNC->isPublic()) continue;

        for (size_t pidx = 0; pidx < FUNC->parameters().size(); ++pidx)
        {
            auto PARAM = FUNC->parameters()[pidx];

            ostringstream pname;
            pname << "c" << cid << "_f" << fidx << "_a" << pidx;
            if (!PARAM->name().empty()) pname << "_" << PARAM->name();

            fparams[PARAM.get()] = make_shared<CVarDecl>(
                _converter.get_type(*PARAM), pname.str()
            );
        }

        fnums[FUNC] = _fids.next();
    }
}

// -------------------------------------------------------------------------- //

list<MainFunctionGenerator::Actor> MainFunctionGenerator::analyze_decls(
    vector<ContractDefinition const*> const& _contracts
) const
{
    list<Actor> actors;
    TicketSystem<uint16_t> cids;
    TicketSystem<uint16_t> fids;

    for (auto contract : _contracts)
    {
        actors.emplace_back(m_converter, contract, nullptr, cids, fids);

        auto const& DECL = actors.back().decl;
        analyze_nested_decls(actors, DECL->id(), contract, cids, fids);
    }

    return actors;
}

void MainFunctionGenerator::analyze_nested_decls(
    list<Actor> & _actors,
    CExprPtr _path,
    ContractDefinition const* _parent,
    TicketSystem<uint16_t> & _cids,
    TicketSystem<uint16_t> & _fids
) const
{
    auto const& children = m_new_graph.children_of(_parent);

    for (auto child : children)
    {
        auto const NAME = VariableScopeResolver::rewrite(
            child.dest->name(), false, VarContext::STRUCT
        );
        auto const PATH = make_shared<CMemberAccess>(_path, NAME);

        _actors.emplace_back(m_converter, child.type, PATH, _cids, _fids);
        analyze_nested_decls(_actors, PATH, child.type, _cids, _fids);
    }
}

// -------------------------------------------------------------------------- //

CStmtPtr MainFunctionGenerator::init_contract(
    ContractDefinition const& _contract, shared_ptr<const CVarDecl> _id
)
{
    CFuncCallBuilder init_builder("Init_" + m_converter.get_name(_contract));

    if (auto ctor = _contract.constructor())
    {
        init_builder.push(make_shared<CReference>(_id->id()));
        m_statedata.push_state_to(init_builder);
        for (auto const param : ctor->parameters())
        {
            string const MSG
                = "Init field " + param->name() + " in " + _contract.name();
            init_builder.push(m_converter.get_nd_val(*param, MSG));
        }
    }

    return _id->assign(init_builder.merge_and_pop())->stmt();
}

// -------------------------------------------------------------------------- //

CBlockList MainFunctionGenerator::build_case(
    FunctionDefinition const& _def,
    map<VariableDeclaration const*, shared_ptr<CVarDecl>> & _args,
    shared_ptr<const CVarDecl> _id
)
{
    auto const& root = FunctionUtilities::extract_root(_def);

    CExprPtr id = _id->id();
    if (!id->is_pointer())
    {
        id = make_shared<CReference>(id);
    }

    CFuncCallBuilder call_builder(m_converter.get_name(root));
    call_builder.push(id);
    m_statedata.push_state_to(call_builder);

    CBlockList call_body;
    for (auto arg : _def.parameters())
    {
        string const MSG = "Set " + arg->name() + " for call " + _def.name();
        auto const& PDECL = _args[arg.get()];
        auto const ND_VAL = m_converter.get_nd_val(*arg, MSG);
        call_body.push_back(PDECL->assign(ND_VAL)->stmt());
        call_builder.push(PDECL->id());
    }
    call_body.push_back(call_builder.merge_and_pop_stmt());
    call_body.push_back(make_shared<CBreak>());

    return call_body;
}

// -------------------------------------------------------------------------- //

void MainFunctionGenerator::update_call_state(CBlockList & _stmts)
{
    for (auto fld : m_statedata.order())
    {
        auto state = make_shared<CIdentifier>(fld.name, false);
        auto nd = TypeConverter::raw_simple_nd(*fld.type, fld.name);

        if (fld.field == CallStateUtilities::Field::Block)
        {
            auto tmp_state = make_shared<CIdentifier>(fld.temp, false);
            _stmts.push_back(tmp_state->access("v")->assign(nd)->stmt());
            _stmts.push_back(make_require(make_shared<CBinaryOp>(
                state->access("v"), "<=", tmp_state->access("v")
            )));
            _stmts.push_back(state->assign(tmp_state)->stmt());
        }
        else
        {
            _stmts.push_back(state->assign(nd)->stmt());
            if (fld.field == CallStateUtilities::Field::Sender)
            {
                _stmts.push_back(make_shared<CBinaryOp>(
                    state->access("v"), "!=", Literals::ZERO
                )->stmt());
            }
        }
    }
}

// -------------------------------------------------------------------------- //

CExprPtr MainFunctionGenerator::get_nd_byte(string const& _msg)
{
    return make_shared<CFuncCall>(
        "rt_nd_byte", CArgList{make_shared<CStringLiteral>(_msg)}
    );
}

// -------------------------------------------------------------------------- //

}
}
}
