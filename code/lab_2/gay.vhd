library ieee;
use ieee.std_logic_1164.all;
entity gate is
    port(
        a,b :in std_logic;
        c :out std_logic
    );
    end gate;

architecture not_g of gate is
    begin
        c <= not a;
    end not_g;

architecture or_g of gate is
    begin
        c <= a or b;
    end or_g;

architecture and_g of gate is
    begin
        c <= a and b;
    end and_g;