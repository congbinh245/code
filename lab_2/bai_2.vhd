library ieee;
use ieee.std_logic_1164.all;

entity Compare_2_bits is 
    port(
        A :in std_logic_vector ( 1 downto 0);
        B :in std_logic_vector ( 1 downto 0);
        GT, EQ, LT :out std_logic;
    );
    end Compare_2_bits;
architecture df of Compare_2_bits is
    begin
        GT <= '1' when A > B else '0';
        EQ <= '1' when A = B else '0';
        LT <= '1' when A < B else '0';
    end df;
architecture bh of Compare_2_bits is
    begin
        process (A,B)
        begin
            if(A > B) then
                GT <= '1';
                EQ <= '0';
                LT <= '0';
            elsif (A = B) then
                GT <= '0';
                EQ <= '1';
                LT <= '0';
            else
                GT <= '0';
                EQ <= '0';
                LT <= '1';
            end if;
        end process;
    end bh;
