enum hi {
 ok,
 not_ok,
};

fn main():int {
    a:[5]Any;
    a[0] = "string";
    a[1] = 1;
    a[2] = 2.2;
    a[3] = 3 as u32;

    t := #type BaseType as &EnumType;
    i:int;
    while i < t.members.length {
        println(itoa(i)+ " " + t.members[i] + " " + itoa(t.values[i] as int));
        i = i + 1;
    }

    assert((hi.c as int) == 4);

    enum hi:u8 {
        a = 2,
        b,
        c,
        d,
    };
    return 0;
}