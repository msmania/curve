all:
	@pushd curvecore & nmake /nologo & popd
	@pushd curve & nmake /nologo & popd

clean:
	@pushd curvecore & nmake /nologo clean & popd
	@pushd curve & nmake /nologo clean & popd
