class VM32Component final : public IComponent {
public:
    void init(SystemBus& bus) override;
    void start() override;
    void stop() override;

private:
    VM32Engine engine;
};
