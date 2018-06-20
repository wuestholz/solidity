#include <libsolidity/ast/ASTBoogieConverter.h>

using namespace std;
using namespace dev;
using namespace dev::solidity;

namespace dev {
namespace solidity {

void ASTBoogieConverter::convert(ASTNode const& _node) {
	_node.accept(*this);
}

bool ASTBoogieConverter::visitNode(ASTNode const&) {
	return true;
}

}
}
