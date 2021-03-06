// test
// assert(false);
/* block comments ?
 * */
/* nest/*
 * ed*/*/

x:int = 2;
x1:int = 2;
x2:string = "hi";

fn to_be_passed() -> bool {
    return false;
}

assigned:fn() -> bool;

fn to_pass_to(f:fn() -> bool) {
    g:fn() -> bool;
    g = f;
    assert(!f());
}

fn testInitializers() -> bool {
    if x := "testing"; x[2] == "e" {
        assert(false);
    } else {
        assert(x == "testing");
    }
    fn incr(x: &$T) -> T {
        old := *x;
        *x += 1;
        return old;
    }
    while i := 0; incr(&i) < 10 {
        assert(i > 0);
    }
    return true;
}

fn main() -> int {
    assert(2 == 2);
    assert(1 + 2 == 3);
    assert(2 * 4 ==  16 / 2);
    assert("test" == "te" + "st");
    x:string;
    x = "123";
    assert(x != "1234");

    assigned = to_be_passed;
    to_pass_to(assigned);

    z := fn () -> bool {
        return true;
    };
    z2 := z;
    assert(z());
    assert(z2());
    if x != "123" {
        assert(false);
    }
    y:string = "12";
    if x == y + "3" {
        assert(true);
    } else {
        assert(false);
    }
    println(x);

    blah := fn(x:int, y:int) -> int {
        return x * x + y * y;
    };
    assert(blah(3, 4) == 25);
    blah2 := fn() -> fn(bool) -> bool {
        return fn(a:bool) -> bool {
            return !a;
        };
    };
    blah3 := blah2();
    assert(blah3(false));
    assert(blah3(!true));
    assert(x1 == 2);
    x1 = x1 + 1;
    assert(x1 == 3);

    if x1 == 2 {
        assert(false);
    } else if x1 == 4 {
        assert(false);
    } else if x1 == 3 {
        assert(true);
    } else {
        assert(false);
    }

    // "anonymous" scope
    a := 1;
    {
        a := "test";
        assert(true);
        {
            println(a);
            a := 2.3;
        }
    }

    s := "some kinda big ol' string";

    s1 := s[2:1];
    assert(s1 == "m");
    s1 = s[:];
    assert(s == s1);

    assert("\r\n"[0] == 13);

    assert((10 as u8) == "\n");

    println("Tests passed.");

    assert(testInitializers());

    return 0;
}
