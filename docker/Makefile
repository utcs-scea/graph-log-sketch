
docker-build:
	mkdir -p ../docker-build
	docker build -t ls_csr:0.1.0 .
	docker run -v $(shell pwd)/../:/LS_CSR -ti ls_csr:0.1.0
