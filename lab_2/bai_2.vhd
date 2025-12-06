library ieee;
use ieee.std_logic_1164.all;

entity Compare_2_bits is 
    port(
        A :in std_logic_vector ( 1 downto 0);
        B :in std_logic_vector ( 1 downto 0);
        S : in std_logic;
        S_mux : in std_logic;
        Y :out std_logic_vector (1 downto 0);
        GT, EQ, LT :out std_logic
    );
    end Compare_2_bits;
architecture dataflow of Compare_2_bits is
    signal tmp_1, tmp_2, tmp_3, tmp_4, tmp_5, tmp_6, tmp_7, tmp_8 : std_logic;
    begin
        -- Mux
        Y(1) <= not(S) and ((not(S_mux) and A(1)) or (S_mux and B(1)));
        Y(0) <= not(S) and ((not(S_mux) and A(0)) or (S_mux and B(0)));
        -- Compare
        -- A > B
        tmp_1 <= (A(1) xnor B(1));
        tmp_2 <= A(1) and not(B(1));
        tmp_3 <= tmp_1 and A(0) and not(B(0));
        GT <= S and ( tmp_2 or tmp_3);
        -- A = B
        tmp_4 <= A(1) xnor B(1);
        tmp_5 <= A(0) xnor B(0);
        EQ <= S and (tmp_4 and tmp_5);
        -- A < B
        tmp_6 <= not(A(1)) and B(1);
        tmp_7 <= not(A(1)) and not(A(0)) and B(0);
        tmp_8 <= not(A(0)) and B(1) and B(0);
        LT <= S and (tmp_6 or tmp_7 or tmp_8);
    end dataflow;