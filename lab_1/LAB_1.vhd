library IEEE;
use IEEE.std_logic_1164.all;
entity lab_1 is
    Port ( 
        a : in STD_LOGIC;
        b : in STD_LOGIC;
        c : in STD_LOGIC;
        y : out STD_LOGIC
    );
end lab_1;
architecture Behavioral of lab_1 is
begin
    y <= (a xor b) OR (a AND (NOT b) AND c) OR (NOT c);
end Behavioral;