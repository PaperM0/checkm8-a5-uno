@echo off
title checkm8 live log
chcp 65001 >nul
powershell -NoProfile -NoExit -Command "Get-Content -LiteralPath 'C:\Users\paper.DESKTOP-7930UDL\Documents\deepseek-harness\default-workspace\atv-checkm8\monitor.log' -Wait -Tail 60 -Encoding UTF8"

