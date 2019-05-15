pragma solidity >=0.5.0;

contract ArrayLayout {

  struct S {
    int x;
  }

  S[] m_a; // No aliasing inside

  function() external payable {
    S[] memory a; // Memory array can have pointers inside
    S memory f = S(1);

    a = new S[](2);
    a[0] = f;
    a[1] = f;
    // Both array elements are memory references
    assert(a[0].x == a[1].x);

    a[0].x = 2;
    // Since they are both references, they are still same
    assert(a[0].x == a[1].x);

    m_a.push(S(1));
    m_a.push(m_a[0]);
    // Storage arrays are no-alias ATDs, no references, copying
    assert(m_a[0].x == m_a[1].x);

    m_a[0].x = 2;
    // We change copies, so different
    assert(m_a[0].x != m_a[1].x);

    S storage a0 = m_a[0];
    S storage a1 = m_a[1];
    a0 = a1; // This assignment is just reference assignment
    a0.x = 3;
    assert(m_a[0].x == 2);
    assert(m_a[1].x == 3);

    a = m_a; // Creates a new array and assigns m_a to it
    assert(a[0].x == 2);
    assert(a[1].x == 3);

    S[][] memory a2 = new S[][](2);
    a2[0] = a; // Reference to a
    a2[1] = a; // Reference to a
    assert(a2[0][0].x == 2 && a2[0][1].x == 3);
    assert(a2[1][0].x == 2 && a2[1][1].x == 3);
    a[0].x = 1; // Changing both elements of a2? YES.
    assert(a2[0][0].x == 1 && a2[0][1].x == 3);
    assert(a2[1][0].x == 1 && a2[1][1].x == 3);

    m_a[0].x = 0;
    m_a[1].x = 0;
    a2[1] = m_a; // Change a? NO. Change both a2[0]? NO.
    assert(a[0].x == 1 && a[1].x == 3);
    assert(a2[0][0].x == 1 && a2[0][1].x == 3);
    assert(a2[1][0].x == 0 && a2[1][1].x == 0);

    assembly {

    }

  }
}
