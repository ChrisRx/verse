type wut : struct {
    x:int;
    y:string;
    z:fn(int,int) -> int;
};
fn check(x:wut) {
    print_str("within check: " + x.y + " " + itoa(x.x) + "\n");
    x.y = x.y + "lol";
    x.x = x.x + 1;
    print_str("within check: " + x.y + " " + itoa(x.x) + "\n");
}

fn main() -> int {
    b:wut;
    b.x = 1;
    b.y = "test";
    b.z = fn (a:int,b:int) -> int {
        return a*b;
    };
    a:&string = &b.y;
    c:&int = &b.x;
    assert(b.y == *a);
    assert(b.x == *c);
    check(b);
    assert(b.y == *a);
    assert(b.x == *c);
    assert(b.z(1,2) == 2);
    wee:bool = fn () -> bool {
        print_str("weeee\n");
        return true;
    }();
    assert(wee);
    assert(!(fn () -> bool {
        return false;
    }()));
    p:ptr;
    assert(!validptr(p));
    p = &b as ptr;
    assert(validptr(p));
    assert(validptr(&b.x as ptr));
    print_str("Success!\n");
    return *c - 1;
}
