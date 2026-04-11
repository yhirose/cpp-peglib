#[test]
fn test_calc() {
    let mut parser = peglib::Parser::new(r#"
        Additive       <- Multiplicative '+' Additive / Multiplicative
        Multiplicative <- Primary '*' Multiplicative / Primary
        Primary        <- '(' Additive ')' / Number
        Number         <- < [0-9]+ >
        %whitespace    <- [ \t]*
    "#).unwrap();

    parser.set_action("Additive", |sv| match sv.choice() {
        0 => sv.get::<i32>(0) + sv.get::<i32>(1),
        _ => sv.get::<i32>(0),
    });

    parser.set_action("Multiplicative", |sv| match sv.choice() {
        0 => sv.get::<i32>(0) * sv.get::<i32>(1),
        _ => sv.get::<i32>(0),
    });

    parser.set_action("Number", |sv| sv.token_to_number::<i32>());

    assert_eq!(parser.parse_into::<i32>("1").unwrap(), 1);
    assert_eq!(parser.parse_into::<i32>("1 + 2").unwrap(), 3);
    assert_eq!(parser.parse_into::<i32>("1 + 2 * 3").unwrap(), 7);
    assert_eq!(parser.parse_into::<i32>("(1 + 2) * 3").unwrap(), 9);
    assert_eq!(parser.parse_into::<i32>("1 * 2 + 3 * 4").unwrap(), 14);
}
