class VM32Engine {
public:
    void init(const VM32Config& cfg, ByteSpan program);
    void executeCycle();
    void shutdown();
};
