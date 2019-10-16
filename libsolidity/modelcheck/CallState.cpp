/**
 * @date 2019
 * First-pass visitor for generating the CallState of Solidity in C models,
 * which consist of the struct of CallState.
 */

#include <libsolidity/modelcheck/CallState.h>
#include <libsolidity/modelcheck/PrimitiveTypeGenerator.h>
#include <libsolidity/modelcheck/Utility.h>
#include <sstream>

using namespace std;

namespace dev
{
namespace solidity
{
namespace modelcheck
{

// -------------------------------------------------------------------------- //

CallState::CallState(
    ASTNode const& _ast,
	TypeConverter const& _converter,
    bool _forward_declare
): m_ast(_ast), m_converter(_converter), m_forward_declare(_forward_declare)
{
}

// -------------------------------------------------------------------------- //

void CallState::print(ostream& _stream)
{
	ScopedSwap<ostream*> stream_swap(m_ostream, &_stream);
    m_ast.accept(*this);
}

// -------------------------------------------------------------------------- //

void CallState::register_primitives(PrimitiveTypeGenerator& _gen)
{
    // TODO(scottwe): See below; this should not be hard-coded...
    _gen.record_address();
    _gen.record_uint(256);
}

// -------------------------------------------------------------------------- //

void CallState::endVisit(ContractDefinition const& _node)
{
    (void) _node;
    // TODO(scottwe): Required fields should be discovered.
    (*m_ostream) << "struct CallState";
    if (!m_forward_declare)
    {
        AddressType addr_type(StateMutability::Payable);
        IntegerType uint256_type(256);

        (*m_ostream) << "{";
        (*m_ostream) << m_converter.get_simple_ctype(addr_type)
                     << " sender;";
        (*m_ostream) << m_converter.get_simple_ctype(uint256_type)
                     << " value;";
        (*m_ostream) << m_converter.get_simple_ctype(uint256_type)
                     << " blocknum;";
        (*m_ostream) << "}";
    }
    (*m_ostream) << ";";
}

// -------------------------------------------------------------------------- //

}
}
}