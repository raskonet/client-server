module license_guard;
import "DPI-C" context function int check_license_dpi();

initial begin
  if(check_license_dpi()==0) begin
    $display("\n[License Guard] ---------------------------------------------");
    $display("[License Guard] FATAL: No License found.");
    $display("[License Guard] ---------------------------------------------\n");
    $finish; 
    
  end
  $display("\nLicense found continuing...\n");
end


endmodule
