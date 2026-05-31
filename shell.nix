{ pkgs ? import <nixpkgs> {} }:
pkgs.mkShell rec {
  nativeBuildInputs = with pkgs; [
    pkg-config gcc clang clang-tools bear
  ];
  buildInputs = with pkgs; [
    libGL
  ] ++ (with pkgs.xorg; [
    libx11 libxi libxcursor libxrandr libxext libxext libxfixes
  ]);

  shellHook = ''
      export C_INCLUDE_PATH="${pkgs.glibc.dev}/include:$C_INCLUDE_PATH"
    '';
}
