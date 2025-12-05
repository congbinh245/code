library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all; -- Cần thiết nếu bạn muốn sử dụng to_integer để in giá trị thập phân

-- Testbench không có cổng (ports)
entity tb_Compare_2_bits is
end tb_Compare_2_bits;

architecture tb_behavior of tb_Compare_2_bits is

    -- Khai báo Component (DUT - Device Under Test)
    component Compare_2_bits
        port(
            A : in std_logic_vector (1 downto 0);
            B : in std_logic_vector (1 downto 0);
            GT, EQ, LT : out std_logic
        );
    end component;

    -- Khai báo tín hiệu cho cổng DUT
    signal A_sig : std_logic_vector (1 downto 0) := "00";
    signal B_sig : std_logic_vector (1 downto 0) := "00";
    signal GT_sig, EQ_sig, LT_sig : std_logic;

    -- Hằng số định nghĩa độ trễ (cho mục đích mô phỏng)
    constant C_CLK_PERIOD : time := 10 ns;

begin

    -- 1. Ánh xạ DUT (Device Under Test)
    UUT : Compare_2_bits
        port map (
            A  => A_sig,
            B  => B_sig,
            GT => GT_sig,
            EQ => EQ_sig,
            LT => LT_sig
        );

    -- 2. Quá trình tạo đầu vào Stimulus (kích thích)
    stim_proc: process
        variable A_int : integer range 0 to 3;
        variable B_int : integer range 0 to 3;
    begin

        -- In tiêu đề cho kết quả mô phỏng
        report "--- STARTING SIMULATION ---" severity NOTE;
        report "   A | B | A>B | A=B | A<B | Expected" severity NOTE;
        report "-------------------------------------" severity NOTE;

        -- Lặp qua tất cả 16 tổ hợp đầu vào
        for A_int in 0 to 3 loop
            A_sig <= std_logic_vector(to_unsigned(A_int, 2)); -- Gán giá trị A

            for B_int in 0 to 3 loop
                B_sig <= std_logic_vector(to_unsigned(B_int, 2)); -- Gán giá trị B

                -- Chờ một khoảng thời gian để mạch xử lý (propagation delay)
                wait for C_CLK_PERIOD;

                -- Logic kiểm tra (Kiểm tra xem đầu ra có đúng với kỳ vọng không)
                if A_int > B_int then
                    assert (GT_sig = '1' and EQ_sig = '0' and LT_sig = '0')
                    report "Test FAILED: A > B, but output is incorrect." severity ERROR;
                    report "   " & integer'image(A_int) & " | " & integer'image(B_int) & " | " & std_logic'image(GT_sig) & " | " & std_logic'image(EQ_sig) & " | " & std_logic'image(LT_sig) & " | GT=1" severity NOTE;

                elsif A_int = B_int then
                    assert (GT_sig = '0' and EQ_sig = '1' and LT_sig = '0')
                    report "Test FAILED: A = B, but output is incorrect." severity ERROR;
                    report "   " & integer'image(A_int) & " | " & integer'image(B_int) & " | " & std_logic'image(GT_sig) & " | " & std_logic'image(EQ_sig) & " | " & std_logic'image(LT_sig) & " | EQ=1" severity NOTE;

                else -- A_int < B_int
                    assert (GT_sig = '0' and EQ_sig = '0' and LT_sig = '1')
                    report "Test FAILED: A < B, but output is incorrect." severity ERROR;
                    report "   " & integer'image(A_int) & " | " & integer'image(B_int) & " | " & std_logic'image(GT_sig) & " | " & std_logic'image(EQ_sig) & " | " & std_logic'image(LT_sig) & " | LT=1" severity NOTE;

                end if;

            end loop; -- Kết thúc B
        end loop; -- Kết thúc A

        report "--- ENDING SIMULATION ---" severity NOTE;
        wait; -- Dừng quá trình mô phỏng
    end process;

end tb_behavior;