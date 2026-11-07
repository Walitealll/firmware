# Run with the target suspended. No target memory writes or function calls.
# Keep the current Debug session so the BNO calibration is not reset.

if g_bno_quality_external.ready == 0
  echo External is already not ready. Current wait_reason (hex), calibration and quality:\n
  p /x g_bno_quality_external.wait_reason
  p g_calib_external
  p g_bno_quality_external
else
  watch -location g_bno_quality_external.ready
  commands
    silent
    if g_bno_quality_external.ready == 0
      echo External ready changed to 0. wait_reason (hex), calibration and quality:\n
      p /x g_bno_quality_external.wait_reason
      p g_calib_external
      p g_bno_quality_external
    else
      continue
    end
  end
  echo Watchpoint installed. Resume (F8) to catch the next loss of readiness.\n
end
