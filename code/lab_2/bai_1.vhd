library ieee;
use ieee.std_logic_1164.all;
entity mux is
    port(
        D0 :in std_logic;
        D1 :in std_logic;
        S :in std_logic;
        y :out std_logic
    );
end mux;
architecture bh of mux is
    begin
        process(D0,D1,S)
            begin
                if(S = '0') then
                    y <= D0;
                else
                    y <= D1;
                end if;
            end process;
    end bh;
architecture fl of mux is
    begin
        y <= (not(S) and D0) or (S and D1);
    end fl;
architecture struc of mux is
    
    signal not_s, out1_and, out2_and : std_logic;
    begin
        not_inst : entity work.gate(not_g)
        port map(
            a => S,
            b => '0',       
            c => not_s

        );
        and1_inst : entity work.gate(and_g)
        port map(
            a => not_s,
            b => D0,
            c => out1_and
        );
        and2_inst : entity work.gate(and_g)
        port map(
            a => S,
            b => D1,
            c => out2_and
        );
        or_inst : entity work.gate(or_g)
        port map(
            a => out1_and,
            b => out2_and,
            c => y
        );
    end struc;



