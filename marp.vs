extern fn print_str(string);
extern fn itoa(int):string;

struct vec3 {
    x:int;
    y:int;
    z:int;
}

fn doathing(a:^vec3) {
    a.x = a.x + a.y * a.z;
}

fn main():int {
    a:auto = hold vec3 {
        x = 1,
        y = 2,
        z = 3
    };
    doathing(a);
    print_str("test: " + itoa(a.x) + " " + itoa(a.y) + " " + itoa(a.z));
    print_str("\n-------\n");
    release a;
    return 1;
}
