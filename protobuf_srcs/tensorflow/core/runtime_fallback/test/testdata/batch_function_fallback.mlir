func.func @f(%arg0: !tfrt.chain, %arg1: !tfrt_fallback.tf_tensor, %arg2: !tfrt_fallback.tf_tensor, %arg3: !tfrt_fallback.tf_tensor, %arg4: !tfrt_fallback.tf_tensor,
        %arg5: !tfrt_fallback.tf_tensor, %arg6: !tfrt_fallback.tf_tensor, %arg7: !tfrt_fallback.tf_tensor, %arg8: !tfrt_fallback.tf_tensor, %arg9: !tfrt_fallback.tf_tensor,
        %arg10: !tfrt_fallback.tf_tensor, %arg11: !tfrt_fallback.tf_tensor, %arg12: !tfrt_fallback.tf_tensor, %arg13: !tfrt_fallback.tf_tensor, %arg14: !tfrt_fallback.tf_tensor,
        %arg15: !tfrt_fallback.tf_tensor, %arg16: !tfrt_fallback.tf_tensor, %arg17: !tfrt_fallback.tf_tensor, %arg18: !tfrt_fallback.tf_tensor, %arg19: !tfrt_fallback.tf_tensor,
        %arg20: !tfrt_fallback.tf_tensor, %arg21: !tfrt_fallback.tf_tensor, %arg22: !tfrt_fallback.tf_tensor, %arg23: !tfrt_fallback.tf_tensor, %arg24: !tfrt_fallback.tf_tensor,
        %arg25: !tfrt_fallback.tf_tensor, %arg26: !tfrt_fallback.tf_tensor, %arg27: !tfrt_fallback.tf_tensor, %arg28: !tfrt_fallback.tf_tensor, %arg29: !tfrt_fallback.tf_tensor,
        %arg30: !tfrt_fallback.tf_tensor, %arg31: !tfrt_fallback.tf_tensor, %arg32: !tfrt_fallback.tf_tensor, %arg33: !tfrt_fallback.tf_tensor, %arg34: !tfrt_fallback.tf_tensor,
        %arg35: !tfrt_fallback.tf_tensor, %arg36: !tfrt_fallback.tf_tensor, %arg37: !tfrt_fallback.tf_tensor, %arg38: !tfrt_fallback.tf_tensor, %arg39: !tfrt_fallback.tf_tensor,
        %arg40: !tfrt_fallback.tf_tensor, %arg41: !tfrt_fallback.tf_tensor, %arg42: !tfrt_fallback.tf_tensor, %arg43: !tfrt_fallback.tf_tensor, %arg44: !tfrt_fallback.tf_tensor,
        %arg45: !tfrt_fallback.tf_tensor, %arg46: !tfrt_fallback.tf_tensor, %arg47: !tfrt_fallback.tf_tensor, %arg48: !tfrt_fallback.tf_tensor, %arg49: !tfrt_fallback.tf_tensor,
        %arg50: !tfrt_fallback.tf_tensor, %arg51: !tfrt_fallback.tf_tensor, %arg52: !tfrt_fallback.tf_tensor, %arg53: !tfrt_fallback.tf_tensor, %arg54: !tfrt_fallback.tf_tensor,
        %arg55: !tfrt_fallback.tf_tensor, %arg56: !tfrt_fallback.tf_tensor, %arg57: !tfrt_fallback.tf_tensor, %arg58: !tfrt_fallback.tf_tensor, %arg59: !tfrt_fallback.tf_tensor,
        %arg60: !tfrt_fallback.tf_tensor, %arg61: !tfrt_fallback.tf_tensor, %arg62: !tfrt_fallback.tf_tensor, %arg63: !tfrt_fallback.tf_tensor, %arg64: !tfrt_fallback.tf_tensor,
        %arg65: !tfrt_fallback.tf_tensor, %arg66: !tfrt_fallback.tf_tensor, %arg67: !tfrt_fallback.tf_tensor, %arg68: !tfrt_fallback.tf_tensor, %arg69: !tfrt_fallback.tf_tensor,
        %arg70: !tfrt_fallback.tf_tensor, %arg71: !tfrt_fallback.tf_tensor, %arg72: !tfrt_fallback.tf_tensor, %arg73: !tfrt_fallback.tf_tensor, %arg74: !tfrt_fallback.tf_tensor,
        %arg75: !tfrt_fallback.tf_tensor, %arg76: !tfrt_fallback.tf_tensor, %arg77: !tfrt_fallback.tf_tensor, %arg78: !tfrt_fallback.tf_tensor, %arg79: !tfrt_fallback.tf_tensor,
        %arg80: !tfrt_fallback.tf_tensor, %arg81: !tfrt_fallback.tf_tensor, %arg82: !tfrt_fallback.tf_tensor, %arg83: !tfrt_fallback.tf_tensor, %arg84: !tfrt_fallback.tf_tensor,
        %arg85: !tfrt_fallback.tf_tensor, %arg86: !tfrt_fallback.tf_tensor, %arg87: !tfrt_fallback.tf_tensor, %arg88: !tfrt_fallback.tf_tensor, %arg89: !tfrt_fallback.tf_tensor,
        %arg90: !tfrt_fallback.tf_tensor, %arg91: !tfrt_fallback.tf_tensor, %arg92: !tfrt_fallback.tf_tensor, %arg93: !tfrt_fallback.tf_tensor, %arg94: !tfrt_fallback.tf_tensor,
        %arg95: !tfrt_fallback.tf_tensor, %arg96: !tfrt_fallback.tf_tensor, %arg97: !tfrt_fallback.tf_tensor, %arg98: !tfrt_fallback.tf_tensor, %arg99: !tfrt_fallback.tf_tensor,
        %arg100: !tfrt_fallback.tf_tensor, %arg101: !tfrt_fallback.tf_tensor, %arg102: !tfrt_fallback.tf_tensor, %arg103: !tfrt_fallback.tf_tensor, %arg104: !tfrt_fallback.tf_tensor,
        %arg105: !tfrt_fallback.tf_tensor, %arg106: !tfrt_fallback.tf_tensor, %arg107: !tfrt_fallback.tf_tensor, %arg108: !tfrt_fallback.tf_tensor, %arg109: !tfrt_fallback.tf_tensor,
        %arg110: !tfrt_fallback.tf_tensor, %arg111: !tfrt_fallback.tf_tensor, %arg112: !tfrt_fallback.tf_tensor)
          -> (!tfrt.chain, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor,
              !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor,
              !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor,
              !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor,
              !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor,
              !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor,
              !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor,
              !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor,
              !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor,
              !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor,
              !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor,
              !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor,
              !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor,
              !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor,
              !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor,
              !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor) {
  tfrt.return %arg0, %arg1, %arg2, %arg3, %arg4, %arg5, %arg6, %arg7, %arg8, %arg9, %arg10,%arg11, %arg12, %arg13, %arg14,
     %arg15, %arg16, %arg17, %arg18, %arg19, %arg20, %arg21, %arg22, %arg23, %arg24, %arg25, %arg26, %arg27, %arg28, %arg29,
     %arg30, %arg31, %arg32, %arg33, %arg34, %arg35, %arg36, %arg37, %arg38, %arg39, %arg40, %arg41, %arg42, %arg43, %arg44,
     %arg45, %arg46, %arg47, %arg48, %arg49, %arg50, %arg51, %arg52, %arg53, %arg54, %arg55, %arg56, %arg57, %arg58, %arg59,
     %arg60, %arg61, %arg62, %arg63, %arg64, %arg65, %arg66, %arg67, %arg68, %arg69, %arg70, %arg71, %arg72, %arg73, %arg74,
     %arg75, %arg76, %arg77, %arg78, %arg79, %arg80, %arg81, %arg82, %arg83, %arg84, %arg85, %arg86, %arg87, %arg88, %arg89,
     %arg90, %arg91, %arg92, %arg93, %arg94, %arg95, %arg96, %arg97, %arg98, %arg99, %arg100, %arg101, %arg102, %arg103, %arg104,
     %arg105, %arg106, %arg107, %arg108, %arg109, %arg110, %arg111, %arg112
             :!tfrt.chain, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor,
              !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor,
              !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor,
              !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor,
              !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor,
              !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor,
              !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor,
              !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor,
              !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor,
              !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor,
              !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor,
              !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor,
              !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor,
              !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor,
              !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor,
              !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor
}

func.func @main(%arg0: !tfrt.chain, %arg1: !tfrt_fallback.tf_tensor, %arg2: !tfrt_fallback.tf_tensor, %arg3: !tfrt_fallback.tf_tensor, %arg4: !tfrt_fallback.tf_tensor,
        %arg5: !tfrt_fallback.tf_tensor, %arg6: !tfrt_fallback.tf_tensor, %arg7: !tfrt_fallback.tf_tensor, %arg8: !tfrt_fallback.tf_tensor, %arg9: !tfrt_fallback.tf_tensor,
        %arg10: !tfrt_fallback.tf_tensor, %arg11: !tfrt_fallback.tf_tensor, %arg12: !tfrt_fallback.tf_tensor, %arg13: !tfrt_fallback.tf_tensor, %arg14: !tfrt_fallback.tf_tensor,
        %arg15: !tfrt_fallback.tf_tensor, %arg16: !tfrt_fallback.tf_tensor, %arg17: !tfrt_fallback.tf_tensor, %arg18: !tfrt_fallback.tf_tensor, %arg19: !tfrt_fallback.tf_tensor,
        %arg20: !tfrt_fallback.tf_tensor, %arg21: !tfrt_fallback.tf_tensor, %arg22: !tfrt_fallback.tf_tensor, %arg23: !tfrt_fallback.tf_tensor, %arg24: !tfrt_fallback.tf_tensor,
        %arg25: !tfrt_fallback.tf_tensor, %arg26: !tfrt_fallback.tf_tensor, %arg27: !tfrt_fallback.tf_tensor, %arg28: !tfrt_fallback.tf_tensor, %arg29: !tfrt_fallback.tf_tensor,
        %arg30: !tfrt_fallback.tf_tensor, %arg31: !tfrt_fallback.tf_tensor, %arg32: !tfrt_fallback.tf_tensor, %arg33: !tfrt_fallback.tf_tensor, %arg34: !tfrt_fallback.tf_tensor,
        %arg35: !tfrt_fallback.tf_tensor, %arg36: !tfrt_fallback.tf_tensor, %arg37: !tfrt_fallback.tf_tensor, %arg38: !tfrt_fallback.tf_tensor, %arg39: !tfrt_fallback.tf_tensor,
        %arg40: !tfrt_fallback.tf_tensor, %arg41: !tfrt_fallback.tf_tensor, %arg42: !tfrt_fallback.tf_tensor, %arg43: !tfrt_fallback.tf_tensor, %arg44: !tfrt_fallback.tf_tensor,
        %arg45: !tfrt_fallback.tf_tensor, %arg46: !tfrt_fallback.tf_tensor, %arg47: !tfrt_fallback.tf_tensor, %arg48: !tfrt_fallback.tf_tensor, %arg49: !tfrt_fallback.tf_tensor,
        %arg50: !tfrt_fallback.tf_tensor, %arg51: !tfrt_fallback.tf_tensor, %arg52: !tfrt_fallback.tf_tensor, %arg53: !tfrt_fallback.tf_tensor, %arg54: !tfrt_fallback.tf_tensor,
        %arg55: !tfrt_fallback.tf_tensor, %arg56: !tfrt_fallback.tf_tensor, %arg57: !tfrt_fallback.tf_tensor, %arg58: !tfrt_fallback.tf_tensor, %arg59: !tfrt_fallback.tf_tensor,
        %arg60: !tfrt_fallback.tf_tensor, %arg61: !tfrt_fallback.tf_tensor, %arg62: !tfrt_fallback.tf_tensor, %arg63: !tfrt_fallback.tf_tensor, %arg64: !tfrt_fallback.tf_tensor,
        %arg65: !tfrt_fallback.tf_tensor, %arg66: !tfrt_fallback.tf_tensor, %arg67: !tfrt_fallback.tf_tensor, %arg68: !tfrt_fallback.tf_tensor, %arg69: !tfrt_fallback.tf_tensor,
        %arg70: !tfrt_fallback.tf_tensor, %arg71: !tfrt_fallback.tf_tensor, %arg72: !tfrt_fallback.tf_tensor, %arg73: !tfrt_fallback.tf_tensor, %arg74: !tfrt_fallback.tf_tensor,
        %arg75: !tfrt_fallback.tf_tensor, %arg76: !tfrt_fallback.tf_tensor, %arg77: !tfrt_fallback.tf_tensor, %arg78: !tfrt_fallback.tf_tensor, %arg79: !tfrt_fallback.tf_tensor,
        %arg80: !tfrt_fallback.tf_tensor, %arg81: !tfrt_fallback.tf_tensor, %arg82: !tfrt_fallback.tf_tensor, %arg83: !tfrt_fallback.tf_tensor, %arg84: !tfrt_fallback.tf_tensor,
        %arg85: !tfrt_fallback.tf_tensor, %arg86: !tfrt_fallback.tf_tensor, %arg87: !tfrt_fallback.tf_tensor, %arg88: !tfrt_fallback.tf_tensor, %arg89: !tfrt_fallback.tf_tensor,
        %arg90: !tfrt_fallback.tf_tensor, %arg91: !tfrt_fallback.tf_tensor, %arg92: !tfrt_fallback.tf_tensor, %arg93: !tfrt_fallback.tf_tensor, %arg94: !tfrt_fallback.tf_tensor,
        %arg95: !tfrt_fallback.tf_tensor, %arg96: !tfrt_fallback.tf_tensor, %arg97: !tfrt_fallback.tf_tensor, %arg98: !tfrt_fallback.tf_tensor, %arg99: !tfrt_fallback.tf_tensor,
        %arg100: !tfrt_fallback.tf_tensor, %arg101: !tfrt_fallback.tf_tensor, %arg102: !tfrt_fallback.tf_tensor, %arg103: !tfrt_fallback.tf_tensor, %arg104: !tfrt_fallback.tf_tensor,
        %arg105: !tfrt_fallback.tf_tensor, %arg106: !tfrt_fallback.tf_tensor, %arg107: !tfrt_fallback.tf_tensor, %arg108: !tfrt_fallback.tf_tensor, %arg109: !tfrt_fallback.tf_tensor,
        %arg110: !tfrt_fallback.tf_tensor, %arg111: !tfrt_fallback.tf_tensor, %arg112: !tfrt_fallback.tf_tensor)
          -> (!tfrt.chain, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor,
              !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor,
              !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor,
              !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor,
              !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor,
              !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor,
              !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor,
              !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor,
              !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor,
              !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor,
              !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor,
              !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor,
              !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor,
              !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor,
              !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor,
              !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor) {

  %res:112 = tfrt_fallback_async.batch_function device("/device:CPU:0") @f (%arg1, %arg2, %arg3, %arg4, %arg5, %arg6, %arg7, %arg8, %arg9, %arg10,%arg11, %arg12, %arg13, %arg14,
     %arg15, %arg16, %arg17, %arg18, %arg19, %arg20, %arg21, %arg22, %arg23, %arg24, %arg25, %arg26, %arg27, %arg28, %arg29,
     %arg30, %arg31, %arg32, %arg33, %arg34, %arg35, %arg36, %arg37, %arg38, %arg39, %arg40, %arg41, %arg42, %arg43, %arg44,
     %arg45, %arg46, %arg47, %arg48, %arg49, %arg50, %arg51, %arg52, %arg53, %arg54, %arg55, %arg56, %arg57, %arg58, %arg59,
     %arg60, %arg61, %arg62, %arg63, %arg64, %arg65, %arg66, %arg67, %arg68, %arg69, %arg70, %arg71, %arg72, %arg73, %arg74,
     %arg75, %arg76, %arg77, %arg78, %arg79, %arg80, %arg81, %arg82, %arg83, %arg84, %arg85, %arg86, %arg87, %arg88, %arg89,
     %arg90, %arg91, %arg92, %arg93, %arg94, %arg95, %arg96, %arg97, %arg98, %arg99, %arg100, %arg101, %arg102, %arg103, %arg104,
     %arg105, %arg106, %arg107, %arg108, %arg109, %arg110, %arg111, %arg112) {
      num_batch_threads = 16,
      max_batch_size = 1,
      allowed_batch_sizes = [1],
      batch_timeout_micros = 0,
      container = "container",
      shared_name = "shared_name",
      batching_queue = "batching_queue",
      enable_large_batch_splitting = false,
      Tin = [i32, i32, i32, i32, i32],
      Tcaptured = [i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32,
                   i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32,
                   i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32,
                   i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32,
                   i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32,
                   i32, i32, i32, i32, i32, i32, i32],
      Tout = [i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32,
              i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32,
              i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32,
              i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32,
              i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32,
              i32, i32, i32, i32, i32, i32, i32]} : 112

  tfrt.return %arg0, %res#0, %res#1, %res#2, %res#3, %res#4, %res#5, %res#6, %res#7, %res#8, %res#9, %res#10, %res#11, %res#12, %res#13, %res#14,
      %res#15, %res#16, %res#17, %res#18, %res#19, %res#20, %res#21, %res#22, %res#23, %res#24, %res#25, %res#26, %res#27, %res#28, %res#29,
      %res#30, %res#31, %res#32, %res#33, %res#34, %res#35, %res#36, %res#37, %res#38, %res#39, %res#40, %res#41, %res#42, %res#43, %res#44,
      %res#45, %res#46, %res#47, %res#48, %res#49, %res#50, %res#51, %res#52, %res#53, %res#54, %res#55, %res#56, %res#57, %res#58, %res#59,
      %res#60, %res#61, %res#62, %res#63, %res#64, %res#65, %res#66, %res#67, %res#68, %res#69, %res#70, %res#71, %res#72, %res#73, %res#74,
      %res#75, %res#76, %res#77, %res#78, %res#79, %res#80, %res#81, %res#82, %res#83, %res#84, %res#85, %res#86, %res#87, %res#88, %res#89,
      %res#90, %res#91, %res#92, %res#93, %res#94, %res#95, %res#96, %res#97, %res#98, %res#99, %res#100, %res#101, %res#102, %res#103, %res#104,
      %res#105, %res#106, %res#107, %res#108, %res#109, %res#110, %res#111
             :!tfrt.chain, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor,
              !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor,
              !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor,
              !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor,
              !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor,
              !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor,
              !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor,
              !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor,
              !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor,
              !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor,
              !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor,
              !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor,
              !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor,
              !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor,
              !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor,
              !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor, !tfrt_fallback.tf_tensor
}
