
import 'dart:async';
import 'dart:convert';

import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';
import 'package:http/http.dart' as http;

const _configuredBaseUrl = String.fromEnvironment('API_BASE_URL');

const Color _brandPrimary = Color(0xFF0F766E);
const Color _brandSecondary = Color(0xFF0EA5E9);
const Color _brandAccent = Color(0xFFFB923C);
const Color _surfaceSoft = Color(0xFFF2F7F8);

void main() {
  runApp(const HybridMobileApp());
}

class HybridMobileApp extends StatefulWidget {
  const HybridMobileApp({super.key});

  @override
  State<HybridMobileApp> createState() => _HybridMobileAppState();
}

class _HybridMobileAppState extends State<HybridMobileApp> {
  bool _showSplash = true;
  AppSession? _session;

  @override
  void initState() {
    super.initState();
    Timer(const Duration(milliseconds: 1400), () {
      if (!mounted) {
        return;
      }
      setState(() {
        _showSplash = false;
      });
    });
  }

  void _handleLogin(AppSession session) {
    setState(() {
      _session = session;
    });
  }

  void _handleLogout() {
    setState(() {
      _session = null;
    });
  }

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'NOVA ERP Mobile',
      debugShowCheckedModeBanner: false,
      theme: ThemeData(
        useMaterial3: true,
        colorScheme: ColorScheme.fromSeed(seedColor: _brandPrimary),
        scaffoldBackgroundColor: _surfaceSoft,
        appBarTheme: const AppBarTheme(
          backgroundColor: Colors.white,
          foregroundColor: Color(0xFF0F172A),
          elevation: 0,
          surfaceTintColor: Colors.transparent,
        ),
      ),
      home: AnimatedSwitcher(
        duration: const Duration(milliseconds: 320),
        child: _buildCurrentScreen(),
      ),
    );
  }

  Widget _buildCurrentScreen() {
    if (_showSplash) {
      return const SplashScreen(key: ValueKey('splash'));
    }
    if (_session == null) {
      return LoginScreen(
        key: const ValueKey('login'),
        onLogin: _handleLogin,
        initialBaseUrl: defaultApiBaseUrl(),
      );
    }
    return ErpShell(
      key: const ValueKey('erp'),
      session: _session!,
      onLogout: _handleLogout,
    );
  }
}

class SplashScreen extends StatelessWidget {
  const SplashScreen({super.key});

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      body: Container(
        decoration: const BoxDecoration(
          gradient: LinearGradient(
            begin: Alignment.topLeft,
            end: Alignment.bottomRight,
            colors: [Color(0xFF052E2B), Color(0xFF0F766E), Color(0xFF14B8A6)],
          ),
        ),
        child: Stack(
          children: [
            Positioned(
              top: -60,
              right: -30,
              child: _shape(const Color(0x330EA5E9), 200),
            ),
            Positioned(
              bottom: -50,
              left: -40,
              child: _shape(const Color(0x33FB923C), 180),
            ),
            Center(
              child: Column(
                mainAxisSize: MainAxisSize.min,
                children: [
                  const CompanyLogo(size: 92),
                  const SizedBox(height: 18),
                  Text(
                    'NOVA ERP',
                    style: Theme.of(context).textTheme.headlineMedium?.copyWith(
                          color: Colors.white,
                          fontWeight: FontWeight.w800,
                          letterSpacing: 1.2,
                        ),
                  ),
                  const SizedBox(height: 6),
                  Text(
                    'Kurumsal Operasyon Platformu',
                    style: Theme.of(context).textTheme.bodyLarge?.copyWith(
                          color: const Color(0xFFE2E8F0),
                        ),
                  ),
                  const SizedBox(height: 28),
                  const SizedBox(
                    width: 180,
                    child: LinearProgressIndicator(
                      minHeight: 5,
                      backgroundColor: Color(0x33FFFFFF),
                      valueColor: AlwaysStoppedAnimation<Color>(_brandAccent),
                    ),
                  ),
                ],
              ),
            ),
          ],
        ),
      ),
    );
  }

  Widget _shape(Color color, double size) {
    return Container(
      width: size,
      height: size,
      decoration: BoxDecoration(
        color: color,
        borderRadius: BorderRadius.circular(size),
      ),
    );
  }
}

class LoginScreen extends StatefulWidget {
  const LoginScreen({
    super.key,
    required this.onLogin,
    required this.initialBaseUrl,
  });

  final void Function(AppSession session) onLogin;
  final String initialBaseUrl;

  @override
  State<LoginScreen> createState() => _LoginScreenState();
}

class _LoginScreenState extends State<LoginScreen> {
  final TextEditingController _companyController = TextEditingController(
    text: 'Nova Endustri A.S.',
  );
  final TextEditingController _userController = TextEditingController(text: 'erp.operator');
  late final TextEditingController _baseUrlController;
  final TextEditingController _tokenController = TextEditingController();

  bool _isSubmitting = false;
  String? _errorText;

  @override
  void initState() {
    super.initState();
    _baseUrlController = TextEditingController(text: widget.initialBaseUrl);
  }

  @override
  void dispose() {
    _companyController.dispose();
    _userController.dispose();
    _baseUrlController.dispose();
    _tokenController.dispose();
    super.dispose();
  }

  Future<void> _submitLogin() async {
    if (_isSubmitting) {
      return;
    }

    final companyName = _companyController.text.trim();
    final username = _userController.text.trim();
    final baseUrl = _baseUrlController.text.trim();
    final token = _tokenController.text.trim();

    if (companyName.isEmpty || username.isEmpty || baseUrl.isEmpty) {
      setState(() {
        _errorText = 'Sirket, kullanici ve API adresi zorunludur.';
      });
      return;
    }

    setState(() {
      _isSubmitting = true;
      _errorText = null;
    });

    final api = PersonApiClient(
      baseUrl: baseUrl,
      bearerToken: token.isEmpty ? null : token,
    );

    try {
      final authStatus = await api.getAuthStatus();
      if (authStatus.tokenRequired && token.isEmpty) {
        throw const ApiException(
          'Bu API ortami Bearer token gerektiriyor. Lutfen token girin.',
        );
      }
      await api.verifyAccess(authStatus);
      widget.onLogin(
        AppSession(
          companyName: companyName,
          username: username,
          baseUrl: baseUrl,
          token: token.isEmpty ? null : token,
        ),
      );
    } on ApiException catch (error) {
      if (!mounted) {
        return;
      }
      setState(() {
        _errorText = error.message;
      });
    } finally {
      api.dispose();
      if (mounted) {
        setState(() {
          _isSubmitting = false;
        });
      }
    }
  }

  @override
  Widget build(BuildContext context) {
    final wide = MediaQuery.sizeOf(context).width > 940;

    return Scaffold(
      body: Container(
        decoration: const BoxDecoration(
          gradient: LinearGradient(
            begin: Alignment.topCenter,
            end: Alignment.bottomCenter,
            colors: [Color(0xFFEEF7F9), Color(0xFFDDEEF2)],
          ),
        ),
        child: SafeArea(
          child: Center(
            child: SingleChildScrollView(
              padding: const EdgeInsets.all(20),
              child: ConstrainedBox(
                constraints: const BoxConstraints(maxWidth: 1080),
                child: wide
                    ? Row(
                        children: [
                          Expanded(child: _buildBrandPanel()),
                          const SizedBox(width: 24),
                          Expanded(child: _buildLoginPanel()),
                        ],
                      )
                    : _buildLoginPanel(),
              ),
            ),
          ),
        ),
      ),
    );
  }

  Widget _buildBrandPanel() {
    return Container(
      padding: const EdgeInsets.all(28),
      decoration: BoxDecoration(
        gradient: const LinearGradient(
          colors: [Color(0xFF0B3D3A), Color(0xFF0F766E)],
          begin: Alignment.topLeft,
          end: Alignment.bottomRight,
        ),
        borderRadius: BorderRadius.circular(28),
        boxShadow: const [
          BoxShadow(
            color: Color(0x33000000),
            blurRadius: 30,
            offset: Offset(0, 12),
          ),
        ],
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        mainAxisSize: MainAxisSize.min,
        children: [
          const CompanyLogo(size: 74),
          const SizedBox(height: 18),
          const Text(
            'NOVA ERP Mobile',
            style: TextStyle(
              color: Colors.white,
              fontSize: 34,
              fontWeight: FontWeight.w800,
              height: 1.2,
            ),
          ),
          const SizedBox(height: 12),
          const Text(
            'Finans, insan kaynaklari, tedarik ve operasyon sureclerini tek panelde yonetin.',
            style: TextStyle(
              color: Color(0xFFE2E8F0),
              fontSize: 16,
              height: 1.5,
            ),
          ),
          const SizedBox(height: 22),
          Wrap(
            spacing: 10,
            runSpacing: 10,
            children: const [
              _FeatureChip('Canli KPI Takibi'),
              _FeatureChip('Onay Is Akislari'),
              _FeatureChip('Rol Bazli Erisim'),
              _FeatureChip('API Destekli Entegrasyon'),
            ],
          ),
        ],
      ),
    );
  }

  Widget _buildLoginPanel() {
    return Container(
      padding: const EdgeInsets.all(24),
      decoration: BoxDecoration(
        color: Colors.white,
        borderRadius: BorderRadius.circular(26),
        boxShadow: const [
          BoxShadow(
            color: Color(0x220F172A),
            blurRadius: 22,
            offset: Offset(0, 10),
          ),
        ],
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        mainAxisSize: MainAxisSize.min,
        children: [
          const Text(
            'Giris Paneli',
            style: TextStyle(fontSize: 24, fontWeight: FontWeight.w800),
          ),
          const SizedBox(height: 8),
          const Text(
            'ERP paneline erismek icin bilgilerinizi girin.',
            style: TextStyle(color: Color(0xFF475569)),
          ),
          const SizedBox(height: 18),
          TextField(
            controller: _companyController,
            decoration: const InputDecoration(labelText: 'Sirket'),
          ),
          const SizedBox(height: 10),
          TextField(
            controller: _userController,
            decoration: const InputDecoration(labelText: 'Kullanici Adi'),
          ),
          const SizedBox(height: 10),
          TextField(
            controller: _baseUrlController,
            decoration: const InputDecoration(labelText: 'API Base URL'),
          ),
          const SizedBox(height: 10),
          TextField(
            controller: _tokenController,
            decoration: const InputDecoration(
              labelText: 'Bearer Token',
            ),
            onSubmitted: (_) => _submitLogin(),
          ),
          if (_errorText != null) ...[
            const SizedBox(height: 12),
            Container(
              padding: const EdgeInsets.all(10),
              decoration: BoxDecoration(
                color: const Color(0xFFFFF1F2),
                borderRadius: BorderRadius.circular(10),
                border: Border.all(color: const Color(0xFFFDA4AF)),
              ),
              child: Text(
                _errorText!,
                style: const TextStyle(color: Color(0xFF9F1239)),
              ),
            ),
          ],
          const SizedBox(height: 18),
          SizedBox(
            width: double.infinity,
            child: FilledButton.icon(
              onPressed: _isSubmitting ? null : _submitLogin,
              icon: _isSubmitting
                  ? const SizedBox(
                      width: 16,
                      height: 16,
                      child: CircularProgressIndicator(strokeWidth: 2),
                    )
                  : const Icon(Icons.login),
              label: Text(_isSubmitting ? 'Baglaniliyor...' : 'ERP Paneline Giris'),
            ),
          ),
        ],
      ),
    );
  }
}

class ErpShell extends StatefulWidget {
  const ErpShell({
    super.key,
    required this.session,
    required this.onLogout,
  });

  final AppSession session;
  final VoidCallback onLogout;

  @override
  State<ErpShell> createState() => _ErpShellState();
}

class _ErpShellState extends State<ErpShell> {
  int _selectedIndex = 0;
  int _nextProjectId = 3;
  int? _activeProjectId = 1;
  final List<ProjectWorkspace> _projects = <ProjectWorkspace>[
    const ProjectWorkspace(
      id: 1,
      code: 'PRJ-001',
      name: 'Hybrid ERP Rollout',
      customer: 'Nova Endustri',
      plannedBudget: 36800,
      actualBudget: 21640,
    ),
    const ProjectWorkspace(
      id: 2,
      code: 'PRJ-002',
      name: 'Depo Otomasyon Faz 2',
      customer: 'Nova Endustri',
      plannedBudget: 18200,
      actualBudget: 9040,
    ),
  ];

  static const List<_ModuleMeta> _modules = [
    _ModuleMeta('Dashboard', Icons.dashboard_customize_rounded),
    _ModuleMeta('Insan Kaynaklari', Icons.groups_rounded),
    _ModuleMeta('Butce', Icons.account_balance_rounded),
    _ModuleMeta('Operasyon Hub', Icons.precision_manufacturing_rounded),
    _ModuleMeta('Raporlama', Icons.analytics_rounded),
  ];

  ProjectWorkspace? get _activeProject {
    for (final project in _projects) {
      if (project.id == _activeProjectId) {
        return project;
      }
    }
    return null;
  }

  void _handleProjectSelect(int? projectId) {
    setState(() {
      _activeProjectId = projectId;
    });
  }

  void _handleProjectCreate({
    required String code,
    required String name,
    required String customer,
    required double plannedBudget,
  }) {
    setState(() {
      final project = ProjectWorkspace(
        id: _nextProjectId,
        code: code,
        name: name,
        customer: customer,
        plannedBudget: plannedBudget,
        actualBudget: 0,
      );
      _nextProjectId += 1;
      _projects.add(project);
      _activeProjectId = project.id;
    });
  }

  void _handleProjectBudgetUpdate({
    required int projectId,
    required double plannedBudget,
    required double actualBudget,
  }) {
    setState(() {
      final index = _projects.indexWhere((project) => project.id == projectId);
      if (index < 0) {
        return;
      }
      _projects[index] = _projects[index].copyWith(
        plannedBudget: plannedBudget,
        actualBudget: actualBudget,
      );
    });
  }

  @override
  Widget build(BuildContext context) {
    final wide = MediaQuery.sizeOf(context).width >= 1080;

    return Scaffold(
      endDrawer: _SettingsDrawer(
        session: widget.session,
        onLogout: widget.onLogout,
      ),
      appBar: AppBar(
        titleSpacing: 14,
        title: Row(
          children: [
            const CompanyLogo(size: 34),
            const SizedBox(width: 10),
            Expanded(
              child: Text(
                '${widget.session.companyName} ERP',
                overflow: TextOverflow.ellipsis,
                style: const TextStyle(fontWeight: FontWeight.w700),
              ),
            ),
          ],
        ),
        actions: [
          Chip(
            label: Text(widget.session.username),
            avatar: const Icon(Icons.verified_user_rounded, size: 18),
          ),
          const SizedBox(width: 8),
          Builder(
            builder: (context) => IconButton(
              tooltip: 'Ayarlar',
              onPressed: () => Scaffold.of(context).openEndDrawer(),
              icon: const Icon(Icons.settings_rounded),
            ),
          ),
          const SizedBox(width: 8),
        ],
      ),
      body: wide ? _buildWideLayout() : _buildCurrentModule(),
      bottomNavigationBar: wide ? null : _buildBottomNav(),
    );
  }

  Widget _buildWideLayout() {
    return Row(
      children: [
        NavigationRail(
          selectedIndex: _selectedIndex,
          onDestinationSelected: (index) {
            setState(() {
              _selectedIndex = index;
            });
          },
          labelType: NavigationRailLabelType.all,
          destinations: _modules
              .map((item) => NavigationRailDestination(
                    icon: Icon(item.icon),
                    label: Text(item.title),
                  ))
              .toList(),
        ),
        const VerticalDivider(width: 1),
        Expanded(child: _buildCurrentModule()),
      ],
    );
  }

  Widget _buildBottomNav() {
    return NavigationBar(
      selectedIndex: _selectedIndex,
      onDestinationSelected: (index) {
        setState(() {
          _selectedIndex = index;
        });
      },
      destinations: _modules
          .map(
            (item) => NavigationDestination(
              icon: Icon(item.icon),
              label: item.title,
            ),
          )
          .toList(),
    );
  }

  Widget _buildCurrentModule() {
    switch (_selectedIndex) {
      case 0:
        return ErpDashboardPage(session: widget.session);
      case 1:
        return HumanResourcesPage(session: widget.session);
      case 2:
        return FinancePage(
          session: widget.session,
          projects: _projects,
          activeProjectId: _activeProjectId,
          onProjectSelect: _handleProjectSelect,
          onProjectBudgetUpdate: _handleProjectBudgetUpdate,
        );
      case 3:
        return OperationsPage(
          session: widget.session,
          projects: _projects,
          activeProjectId: _activeProjectId,
          onProjectSelect: _handleProjectSelect,
          onProjectCreate: _handleProjectCreate,
        );
      case 4:
        return const ReportsPage();
      default:
        return ErpDashboardPage(session: widget.session);
    }
  }
}

class ErpDashboardPage extends StatelessWidget {
  const ErpDashboardPage({super.key, required this.session});

  final AppSession session;

  @override
  Widget build(BuildContext context) {
    return ListView(
      padding: const EdgeInsets.all(16),
      children: [
        _ModuleHeader(
          title: 'Yonetici Dashboard',
          subtitle: 'Hos geldiniz ${session.username}, bugunku ozet metrikler burada.',
        ),
        const SizedBox(height: 12),
        Wrap(
          spacing: 12,
          runSpacing: 12,
          children: const [
            _KpiCard(title: 'Gunluk Ciro', value: 'TL 1.248.000', trend: '+7.2%'),
            _KpiCard(title: 'Acil Onay', value: '14', trend: '-2'),
            _KpiCard(title: 'Acik Is Emri', value: '36', trend: '+5'),
            _KpiCard(title: 'Stok Alarmi', value: '8', trend: '+1'),
          ],
        ),
        const SizedBox(height: 12),
        Row(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Expanded(
              child: _PanelCard(
                title: 'Onay Bekleyen Islemler',
                child: Column(
                  children: const [
                    _ListRow('Satinalma Talebi #PR-2041', 'Mali Isler', 'Yuksek'),
                    _ListRow('Ek Butce Talebi #BG-119', 'Finans', 'Orta'),
                    _ListRow('Vardiya Revizyonu #HR-883', 'IK', 'Dusuk'),
                  ],
                ),
              ),
            ),
            const SizedBox(width: 12),
            Expanded(
              child: _PanelCard(
                title: 'Hizli Islem',
                child: Wrap(
                  spacing: 8,
                  runSpacing: 8,
                  children: const [
                    _ActionBadge('Yeni Talep Olustur'),
                    _ActionBadge('Nakit Akisi'),
                    _ActionBadge('Stok Sayimi'),
                    _ActionBadge('Uretim Plani'),
                    _ActionBadge('Aylik KPI Raporu'),
                  ],
                ),
              ),
            ),
          ],
        ),
      ],
    );
  }
}

class HumanResourcesPage extends StatelessWidget {
  const HumanResourcesPage({super.key, required this.session});

  final AppSession session;

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.all(16),
      child: Column(
        children: [
          const _ModuleHeader(
            title: 'Insan Kaynaklari / Personel Kartlari',
            subtitle: 'Personel olusturma, guncelleme ve silme islemlerini bu panelden yonetin.',
          ),
          const SizedBox(height: 10),
          Wrap(
            spacing: 10,
            runSpacing: 10,
            children: const [
              _MiniStat(label: 'Toplam Personel', value: '324'),
              _MiniStat(label: 'Yeni Ise Alim', value: '12'),
              _MiniStat(label: 'Izin Talepleri', value: '9'),
              _MiniStat(label: 'Acil Pozisyon', value: '5'),
            ],
          ),
          const SizedBox(height: 12),
          Expanded(child: PersonManagementPanel(session: session)),
        ],
      ),
    );
  }
}

class PersonManagementPanel extends StatefulWidget {
  const PersonManagementPanel({super.key, required this.session});

  final AppSession session;

  @override
  State<PersonManagementPanel> createState() => _PersonManagementPanelState();
}

class _PersonManagementPanelState extends State<PersonManagementPanel> {
  late PersonApiClient _api;
  final TextEditingController _firstNameController = TextEditingController();
  final TextEditingController _lastNameController = TextEditingController();
  final TextEditingController _emailController = TextEditingController();
  final TextEditingController _ageController = TextEditingController();

  List<Person> _persons = const <Person>[];
  bool _isLoading = true;
  bool _isSubmitting = false;
  String? _loadError;
  int? _editingPersonId;

  @override
  void initState() {
    super.initState();
    _api = PersonApiClient(
      baseUrl: widget.session.baseUrl,
      bearerToken: widget.session.token,
    );
    _loadPersons();
  }

  @override
  void didUpdateWidget(covariant PersonManagementPanel oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (oldWidget.session.baseUrl != widget.session.baseUrl ||
        oldWidget.session.token != widget.session.token) {
      _api.dispose();
      _api = PersonApiClient(
        baseUrl: widget.session.baseUrl,
        bearerToken: widget.session.token,
      );
      _loadPersons();
    }
  }

  @override
  void dispose() {
    _firstNameController.dispose();
    _lastNameController.dispose();
    _emailController.dispose();
    _ageController.dispose();
    _api.dispose();
    super.dispose();
  }

  Future<void> _loadPersons() async {
    setState(() {
      _isLoading = true;
      _loadError = null;
    });

    try {
      final persons = await _api.listPersons(pageSize: 100);
      if (!mounted) {
        return;
      }
      setState(() {
        _persons = persons;
      });
    } on ApiException catch (error) {
      if (!mounted) {
        return;
      }
      setState(() {
        _loadError = error.message;
      });
    } finally {
      if (!mounted) {
        return;
      }
      setState(() {
        _isLoading = false;
      });
    }
  }

  Future<void> _submitPerson() async {
    if (_isSubmitting) {
      return;
    }

    final firstName = _firstNameController.text.trim();
    final lastName = _lastNameController.text.trim();
    final email = _emailController.text.trim();
    final age = int.tryParse(_ageController.text.trim());

    if (firstName.isEmpty || lastName.isEmpty || email.isEmpty || age == null || age < 0) {
      _showMessage('Lutfen gecerli ad, soyad, e-posta ve yas girin.');
      return;
    }

    final payload = PersonInput(
      firstName: firstName,
      lastName: lastName,
      email: email,
      age: age,
    );

    setState(() {
      _isSubmitting = true;
    });

    try {
      if (_editingPersonId == null) {
        await _api.createPerson(payload);
        _showMessage('Personel karti olusturuldu.');
      } else {
        await _api.updatePerson(_editingPersonId!, payload);
        _showMessage('Personel karti guncellendi.');
      }
      _clearForm();
      await _loadPersons();
    } on ApiException catch (error) {
      _showMessage(error.message);
    } finally {
      if (!mounted) {
        return;
      }
      setState(() {
        _isSubmitting = false;
      });
    }
  }

  void _startEditing(Person person) {
    setState(() {
      _editingPersonId = person.id;
      _firstNameController.text = person.firstName;
      _lastNameController.text = person.lastName;
      _emailController.text = person.email;
      _ageController.text = person.age.toString();
    });
  }

  void _clearForm() {
    setState(() {
      _editingPersonId = null;
      _firstNameController.clear();
      _lastNameController.clear();
      _emailController.clear();
      _ageController.clear();
    });
  }

  Future<void> _deletePerson(Person person) async {
    final shouldDelete = await showDialog<bool>(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('Personel Kaydi Sil'),
        content: Text('${person.firstName} ${person.lastName} kaydi silinsin mi?'),
        actions: [
          TextButton(
            onPressed: () => Navigator.of(context).pop(false),
            child: const Text('Vazgec'),
          ),
          FilledButton(
            onPressed: () => Navigator.of(context).pop(true),
            child: const Text('Sil'),
          ),
        ],
      ),
    );

    if (shouldDelete != true) {
      return;
    }

    try {
      await _api.deletePerson(person.id);
      _showMessage('Personel karti silindi.');
      await _loadPersons();
    } on ApiException catch (error) {
      _showMessage(error.message);
    }
  }

  void _showMessage(String message) {
    if (!mounted) {
      return;
    }
    ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text(message)));
  }

  @override
  Widget build(BuildContext context) {
    return Card(
      elevation: 0,
      child: Padding(
        padding: const EdgeInsets.all(14),
        child: Column(
          children: [
            Row(
              children: [
                Text(
                  _editingPersonId == null
                      ? 'Yeni Personel Kaydi'
                      : 'Personel Duzenle #$_editingPersonId',
                  style: Theme.of(context).textTheme.titleMedium?.copyWith(
                        fontWeight: FontWeight.w700,
                      ),
                ),
                const Spacer(),
                IconButton(
                  tooltip: 'Yenile',
                  onPressed: _loadPersons,
                  icon: const Icon(Icons.refresh_rounded),
                ),
              ],
            ),
            const SizedBox(height: 8),
            Wrap(
              spacing: 8,
              runSpacing: 8,
              children: [
                _sizedInput(_firstNameController, 'Ad', 180),
                _sizedInput(_lastNameController, 'Soyad', 180),
                _sizedInput(
                  _emailController,
                  'E-Posta',
                  250,
                  keyboardType: TextInputType.emailAddress,
                ),
                _sizedInput(
                  _ageController,
                  'Yas',
                  110,
                  keyboardType: TextInputType.number,
                ),
              ],
            ),
            const SizedBox(height: 10),
            Row(
              children: [
                FilledButton.icon(
                  onPressed: _isSubmitting ? null : _submitPerson,
                  icon: _isSubmitting
                      ? const SizedBox(
                          width: 16,
                          height: 16,
                          child: CircularProgressIndicator(strokeWidth: 2),
                        )
                      : const Icon(Icons.save_rounded),
                  label: Text(_editingPersonId == null ? 'Kaydet' : 'Guncelle'),
                ),
                const SizedBox(width: 8),
                OutlinedButton(
                  onPressed: _isSubmitting ? null : _clearForm,
                  child: const Text('Temizle'),
                ),
                const Spacer(),
                Text(
                  'API: ${widget.session.baseUrl}',
                  style: Theme.of(context).textTheme.bodySmall,
                ),
              ],
            ),
            const Divider(height: 22),
            Expanded(child: _buildPersonsList()),
          ],
        ),
      ),
    );
  }

  Widget _sizedInput(
    TextEditingController controller,
    String label,
    double width, {
    TextInputType keyboardType = TextInputType.text,
  }) {
    return SizedBox(
      width: width,
      child: TextField(
        controller: controller,
        keyboardType: keyboardType,
        decoration: InputDecoration(labelText: label),
      ),
    );
  }

  Widget _buildPersonsList() {
    if (_isLoading) {
      return const Center(child: CircularProgressIndicator());
    }
    if (_loadError != null) {
      return Center(
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            Text(_loadError!, textAlign: TextAlign.center),
            const SizedBox(height: 8),
            FilledButton(
              onPressed: _loadPersons,
              child: const Text('Tekrar Dene'),
            ),
          ],
        ),
      );
    }
    if (_persons.isEmpty) {
      return const Center(child: Text('Kayitli personel bulunmadi.'));
    }
    return ListView.separated(
      itemCount: _persons.length,
      separatorBuilder: (_, __) => const Divider(height: 1),
      itemBuilder: (context, index) {
        final person = _persons[index];
        return ListTile(
          leading: CircleAvatar(child: Text(person.id.toString())),
          title: Text('${person.firstName} ${person.lastName}'),
          subtitle: Text('${person.email} • Yas ${person.age}'),
          trailing: Row(
            mainAxisSize: MainAxisSize.min,
            children: [
              IconButton(
                onPressed: () => _startEditing(person),
                icon: const Icon(Icons.edit_rounded),
              ),
              IconButton(
                onPressed: () => _deletePerson(person),
                icon: const Icon(Icons.delete_outline_rounded),
              ),
            ],
          ),
        );
      },
    );
  }
}

class FinancePage extends StatefulWidget {
  const FinancePage({
    super.key,
    required this.session,
    required this.projects,
    required this.activeProjectId,
    required this.onProjectSelect,
    required this.onProjectBudgetUpdate,
  });

  final AppSession session;
  final List<ProjectWorkspace> projects;
  final int? activeProjectId;
  final ValueChanged<int?> onProjectSelect;
  final void Function({
    required int projectId,
    required double plannedBudget,
    required double actualBudget,
  }) onProjectBudgetUpdate;

  @override
  State<FinancePage> createState() => _FinancePageState();
}

class _FinancePageState extends State<FinancePage> {
  final TextEditingController _plannedBudgetController = TextEditingController();
  final TextEditingController _actualBudgetController = TextEditingController();

  ProjectWorkspace? get _activeProject {
    for (final project in widget.projects) {
      if (project.id == widget.activeProjectId) {
        return project;
      }
    }
    return null;
  }

  @override
  void initState() {
    super.initState();
    _syncBudgetControllers();
  }

  @override
  void didUpdateWidget(covariant FinancePage oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (oldWidget.activeProjectId != widget.activeProjectId || oldWidget.projects != widget.projects) {
      _syncBudgetControllers();
    }
  }

  @override
  void dispose() {
    _plannedBudgetController.dispose();
    _actualBudgetController.dispose();
    super.dispose();
  }

  void _syncBudgetControllers() {
    final project = _activeProject;
    if (project == null) {
      _plannedBudgetController.clear();
      _actualBudgetController.clear();
      return;
    }
    _plannedBudgetController.text = project.plannedBudget.toStringAsFixed(0);
    _actualBudgetController.text = project.actualBudget.toStringAsFixed(0);
  }

  void _saveBudget() {
    final project = _activeProject;
    if (project == null) {
      _showMessage('Once bir proje secin.');
      return;
    }

    final planned = double.tryParse(_plannedBudgetController.text.trim());
    final actual = double.tryParse(_actualBudgetController.text.trim());
    if (planned == null || actual == null || planned < 0 || actual < 0) {
      _showMessage('Planlanan ve gerceklesen butce degerleri gecerli olmali.');
      return;
    }

    widget.onProjectBudgetUpdate(
      projectId: project.id,
      plannedBudget: planned,
      actualBudget: actual,
    );
    _showMessage('Proje butcesi guncellendi.');
  }

  void _showMessage(String message) {
    if (!mounted) {
      return;
    }
    ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text(message)));
  }

  @override
  Widget build(BuildContext context) {
    final totalPlanned = widget.projects.fold<double>(0, (sum, item) => sum + item.plannedBudget);
    final totalActual = widget.projects.fold<double>(0, (sum, item) => sum + item.actualBudget);
    final totalVariance = totalPlanned - totalActual;
    final project = _activeProject;

    return ListView(
      padding: const EdgeInsets.all(16),
      children: [
        _ModuleHeader(
          title: 'Butce ve Finans',
          subtitle:
              'Proje bazli ve sirket geneli butceyi ${widget.session.username} icin ozetler.',
        ),
        const SizedBox(height: 12),
        Card(
          elevation: 0,
          child: Padding(
            padding: const EdgeInsets.all(14),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                const Text(
                  'Proje Butce Konteksi',
                  style: TextStyle(fontWeight: FontWeight.w700),
                ),
                const SizedBox(height: 10),
                DropdownButtonFormField<int>(
                  value: widget.activeProjectId,
                  decoration: const InputDecoration(labelText: 'Aktif Proje'),
                  items: widget.projects
                      .map(
                        (item) => DropdownMenuItem<int>(
                          value: item.id,
                          child: Text('${item.code} • ${item.name}'),
                        ),
                      )
                      .toList(),
                  onChanged: widget.onProjectSelect,
                ),
                const SizedBox(height: 10),
                if (project != null)
                  Text(
                    'Musteri: ${project.customer} • Varyans: USD ${project.variance.toStringAsFixed(0)}',
                    style: const TextStyle(color: Color(0xFF475569)),
                  ),
              ],
            ),
          ),
        ),
        const SizedBox(height: 12),
        Wrap(
          spacing: 12,
          runSpacing: 12,
          children: [
            _KpiCard(
              title: 'Sirket Plan Butce',
              value: 'USD ${totalPlanned.toStringAsFixed(0)}',
              trend: '${widget.projects.length} Proje',
            ),
            _KpiCard(
              title: 'Sirket Gerceklesen',
              value: 'USD ${totalActual.toStringAsFixed(0)}',
              trend: 'Toplam Harcama',
            ),
            _KpiCard(
              title: 'Sirket Varyans',
              value: 'USD ${totalVariance.toStringAsFixed(0)}',
              trend: totalVariance >= 0 ? 'Pozitif' : 'Negatif',
            ),
            _KpiCard(
              title: 'Aktif Proje Butcesi',
              value: project == null ? '-' : 'USD ${project.plannedBudget.toStringAsFixed(0)}',
              trend: project == null ? 'Proje secin' : project.code,
            ),
          ],
        ),
        const SizedBox(height: 12),
        _PanelCard(
          title: 'Proje Bazli Butce Guncelleme',
          child: project == null
              ? const Text('Butce duzenlemek icin once bir proje secin.')
              : Column(
                  children: [
                    Wrap(
                      spacing: 8,
                      runSpacing: 8,
                      children: [
                        _budgetInput(_plannedBudgetController, 'Planlanan (USD)', 180),
                        _budgetInput(_actualBudgetController, 'Gerceklesen (USD)', 180),
                        Padding(
                          padding: const EdgeInsets.only(top: 6),
                          child: FilledButton.icon(
                            onPressed: _saveBudget,
                            icon: const Icon(Icons.save_rounded),
                            label: const Text('Butceyi Kaydet'),
                          ),
                        ),
                      ],
                    ),
                    const SizedBox(height: 10),
                    _ListRow(
                      '${project.code} / ${project.name}',
                      'USD ${project.actualBudget.toStringAsFixed(0)}',
                      'USD ${project.variance.toStringAsFixed(0)}',
                    ),
                  ],
                ),
        ),
        const SizedBox(height: 12),
        _PanelCard(
          title: 'Proje Butce Listesi',
          child: Column(
            children: widget.projects
                .map(
                  (item) => _ListRow(
                    '${item.code} / ${item.name}',
                    'Plan ${item.plannedBudget.toStringAsFixed(0)}',
                    'Aktual ${item.actualBudget.toStringAsFixed(0)}',
                  ),
                )
                .toList(),
          ),
        ),
      ],
    );
  }

  Widget _budgetInput(
    TextEditingController controller,
    String label,
    double width,
  ) {
    return SizedBox(
      width: width,
      child: TextField(
        controller: controller,
        keyboardType: TextInputType.number,
        decoration: InputDecoration(labelText: label),
      ),
    );
  }
}

class OperationsPage extends StatefulWidget {
  const OperationsPage({
    super.key,
    required this.session,
    required this.projects,
    required this.activeProjectId,
    required this.onProjectSelect,
    required this.onProjectCreate,
  });

  final AppSession session;
  final List<ProjectWorkspace> projects;
  final int? activeProjectId;
  final ValueChanged<int?> onProjectSelect;
  final void Function({
    required String code,
    required String name,
    required String customer,
    required double plannedBudget,
  }) onProjectCreate;

  @override
  State<OperationsPage> createState() => _OperationsPageState();
}

class _OperationsPageState extends State<OperationsPage> with SingleTickerProviderStateMixin {
  late final TabController _tabController;

  ProjectWorkspace? get _activeProject {
    for (final project in widget.projects) {
      if (project.id == widget.activeProjectId) {
        return project;
      }
    }
    return null;
  }

  @override
  void initState() {
    super.initState();
    _tabController = TabController(length: 4, vsync: this);
  }

  @override
  void dispose() {
    _tabController.dispose();
    super.dispose();
  }

  Future<void> _showCreateProjectDialog() async {
    final codeController = TextEditingController();
    final nameController = TextEditingController();
    final customerController = TextEditingController(text: 'Nova Endustri');
    final plannedBudgetController = TextEditingController(text: '10000');

    final created = await showDialog<bool>(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('Yeni Proje Olustur'),
        content: SizedBox(
          width: 420,
          child: Column(
            mainAxisSize: MainAxisSize.min,
            children: [
              TextField(
                controller: codeController,
                decoration: const InputDecoration(labelText: 'Proje Kodu'),
              ),
              TextField(
                controller: nameController,
                decoration: const InputDecoration(labelText: 'Proje Adi'),
              ),
              TextField(
                controller: customerController,
                decoration: const InputDecoration(labelText: 'Musteri'),
              ),
              TextField(
                controller: plannedBudgetController,
                keyboardType: TextInputType.number,
                decoration: const InputDecoration(labelText: 'Planlanan Butce (USD)'),
              ),
            ],
          ),
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.of(context).pop(false),
            child: const Text('Vazgec'),
          ),
          FilledButton(
            onPressed: () {
              final code = codeController.text.trim();
              final name = nameController.text.trim();
              final customer = customerController.text.trim();
              final plannedBudget = double.tryParse(plannedBudgetController.text.trim());
              if (code.isEmpty || name.isEmpty || customer.isEmpty || plannedBudget == null || plannedBudget < 0) {
                ScaffoldMessenger.of(context).showSnackBar(
                  const SnackBar(content: Text('Proje bilgileri gecerli olmalidir.')),
                );
                return;
              }

              widget.onProjectCreate(
                code: code,
                name: name,
                customer: customer,
                plannedBudget: plannedBudget,
              );
              Navigator.of(context).pop(true);
            },
            child: const Text('Olustur'),
          ),
        ],
      ),
    );

    codeController.dispose();
    nameController.dispose();
    customerController.dispose();
    plannedBudgetController.dispose();

    if (created == true && mounted) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('Proje olusturuldu ve aktif hale getirildi.')),
      );
    }
  }

  @override
  Widget build(BuildContext context) {
    final activeProject = _activeProject;

    return Padding(
      padding: const EdgeInsets.all(16),
      child: Column(
        children: [
          const _ModuleHeader(
            title: 'Operasyon Hub',
            subtitle:
                'Siparis, urun, BOM ve depo stok surecini tek panelden yonetin.',
          ),
          const SizedBox(height: 10),
          Card(
            elevation: 0,
            child: Padding(
              padding: const EdgeInsets.all(14),
              child: Wrap(
                spacing: 8,
                runSpacing: 8,
                crossAxisAlignment: WrapCrossAlignment.center,
                children: [
                  SizedBox(
                    width: 360,
                    child: DropdownButtonFormField<int>(
                      value: widget.activeProjectId,
                      decoration: const InputDecoration(labelText: 'Projeye Giris'),
                      items: widget.projects
                          .map(
                            (item) => DropdownMenuItem<int>(
                              value: item.id,
                              child: Text('${item.code} • ${item.name}'),
                            ),
                          )
                          .toList(),
                      onChanged: widget.onProjectSelect,
                    ),
                  ),
                  FilledButton.icon(
                    onPressed: _showCreateProjectDialog,
                    icon: const Icon(Icons.add_box_outlined),
                    label: const Text('Proje Olustur'),
                  ),
                  if (activeProject != null)
                    Chip(
                      label: Text('Aktif: ${activeProject.code}'),
                      avatar: const Icon(Icons.login_rounded, size: 18),
                    ),
                ],
              ),
            ),
          ),
          const SizedBox(height: 10),
          if (activeProject == null)
            const Expanded(
              child: Center(
                child: Text('Operasyon detaylarini gormek icin once bir proje secin veya olusturun.'),
              ),
            )
          else
            Expanded(
              child: Column(
                children: [
                  Card(
                    elevation: 0,
                    child: TabBar(
                      controller: _tabController,
                      isScrollable: true,
                      tabs: const [
                        Tab(icon: Icon(Icons.receipt_long_rounded), text: 'Siparis'),
                        Tab(icon: Icon(Icons.inventory_2_rounded), text: 'Urun'),
                        Tab(icon: Icon(Icons.account_tree_rounded), text: 'BOM'),
                        Tab(icon: Icon(Icons.warehouse_rounded), text: 'Depo/Stok'),
                      ],
                    ),
                  ),
                  const SizedBox(height: 10),
                  Expanded(
                    child: TabBarView(
                      controller: _tabController,
                      children: [
                        _OrdersPanel(project: activeProject),
                        ProductManagementPanel(session: widget.session),
                        _BomBuilderPanel(project: activeProject),
                        StockManagementPanel(session: widget.session, project: activeProject),
                      ],
                    ),
                  ),
                ],
              ),
            ),
        ],
      ),
    );
  }
}

class _OrdersPanel extends StatelessWidget {
  const _OrdersPanel({required this.project});

  final ProjectWorkspace project;

  @override
  Widget build(BuildContext context) {
    return ListView(
      children: [
        _PanelCard(
          title: '${project.code} / Verilen Siparisler',
          child: Column(
            children: [
              _ListRow('${project.code}-SO-01 / Konsol Kiti', 'Onaylandi', 'Sevkiyata Hazir'),
              _ListRow('${project.code}-SO-02 / Panel Montaj', 'Ayrildi', 'Parca Bekliyor'),
              _ListRow('${project.code}-SO-03 / Endustri Kutusu', 'Sevk Edildi', 'Kapanisa Yakin'),
            ],
          ),
        ),
        const SizedBox(height: 12),
        _PanelCard(
          title: '${project.code} / Satinalma Siparisleri',
          child: Column(
            children: [
              _ListRow('${project.code}-PO-01 / PCB', 'Tedarikci Onayi', 'Yuksek'),
              _ListRow('${project.code}-PO-02 / Kablo Seti', 'Yolda', 'Orta'),
              _ListRow('${project.code}-PO-03 / Vida Seti', 'Kismi Teslim', 'Dusuk'),
            ],
          ),
        ),
      ],
    );
  }
}

class ProductManagementPanel extends StatefulWidget {
  const ProductManagementPanel({super.key, required this.session});

  final AppSession session;

  @override
  State<ProductManagementPanel> createState() => _ProductManagementPanelState();
}

class _ProductManagementPanelState extends State<ProductManagementPanel> {
  late PersonApiClient _api;

  final TextEditingController _skuController = TextEditingController();
  final TextEditingController _nameController = TextEditingController();
  final TextEditingController _categoryController = TextEditingController();
  final TextEditingController _uomController = TextEditingController(text: 'EA');
  final TextEditingController _safetyStockController = TextEditingController(text: '0');
  final TextEditingController _reorderPointController = TextEditingController(text: '0');

  ProductType _productType = ProductType.finished;
  bool _isStockTracked = true;
  bool _isLoading = true;
  bool _isSubmitting = false;
  String? _loadError;
  int? _editingProductId;
  List<ProductItem> _products = const <ProductItem>[];

  @override
  void initState() {
    super.initState();
    _api = PersonApiClient(
      baseUrl: widget.session.baseUrl,
      bearerToken: widget.session.token,
    );
    _loadProducts();
  }

  @override
  void didUpdateWidget(covariant ProductManagementPanel oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (oldWidget.session.baseUrl != widget.session.baseUrl ||
        oldWidget.session.token != widget.session.token) {
      _api.dispose();
      _api = PersonApiClient(
        baseUrl: widget.session.baseUrl,
        bearerToken: widget.session.token,
      );
      _loadProducts();
    }
  }

  @override
  void dispose() {
    _api.dispose();
    _skuController.dispose();
    _nameController.dispose();
    _categoryController.dispose();
    _uomController.dispose();
    _safetyStockController.dispose();
    _reorderPointController.dispose();
    super.dispose();
  }

  Future<void> _loadProducts() async {
    setState(() {
      _isLoading = true;
      _loadError = null;
    });

    try {
      final products = await _api.listProducts(pageSize: 100);
      if (!mounted) {
        return;
      }
      setState(() {
        _products = products;
      });
    } on ApiException catch (error) {
      if (!mounted) {
        return;
      }
      setState(() {
        _loadError = error.message;
      });
    } finally {
      if (!mounted) {
        return;
      }
      setState(() {
        _isLoading = false;
      });
    }
  }

  Future<void> _submitProduct() async {
    if (_isSubmitting) {
      return;
    }

    final sku = _skuController.text.trim();
    final name = _nameController.text.trim();
    final category = _categoryController.text.trim();
    final uom = _uomController.text.trim();
    final safetyStock = int.tryParse(_safetyStockController.text.trim());
    final reorderPoint = int.tryParse(_reorderPointController.text.trim());

    if (sku.isEmpty ||
        name.isEmpty ||
        category.isEmpty ||
        uom.isEmpty ||
        safetyStock == null ||
        reorderPoint == null ||
        safetyStock < 0 ||
        reorderPoint < 0) {
      _showMessage('Lutfen urun formundaki alanlari gecerli doldurun.');
      return;
    }

    final input = ProductInput(
      sku: sku,
      name: name,
      category: category,
      defaultUom: uom,
      productType: _productType,
      isStockTracked: _isStockTracked,
      safetyStock: safetyStock,
      reorderPoint: reorderPoint,
    );

    setState(() {
      _isSubmitting = true;
    });

    try {
      if (_editingProductId == null) {
        await _api.createProduct(input);
        _showMessage('Urun kaydi olusturuldu.');
      } else {
        await _api.updateProduct(_editingProductId!, input);
        _showMessage('Urun kaydi guncellendi.');
      }
      _clearForm();
      await _loadProducts();
    } on ApiException catch (error) {
      _showMessage(error.message);
    } finally {
      if (!mounted) {
        return;
      }
      setState(() {
        _isSubmitting = false;
      });
    }
  }

  void _startEditing(ProductItem product) {
    setState(() {
      _editingProductId = product.id;
      _skuController.text = product.sku;
      _nameController.text = product.name;
      _categoryController.text = product.category;
      _uomController.text = product.defaultUom;
      _productType = product.productType;
      _isStockTracked = product.isStockTracked;
      _safetyStockController.text = product.safetyStock.toString();
      _reorderPointController.text = product.reorderPoint.toString();
    });
  }

  void _clearForm() {
    setState(() {
      _editingProductId = null;
      _skuController.clear();
      _nameController.clear();
      _categoryController.clear();
      _uomController.text = 'EA';
      _productType = ProductType.finished;
      _isStockTracked = true;
      _safetyStockController.text = '0';
      _reorderPointController.text = '0';
    });
  }

  Future<void> _deleteProduct(ProductItem product) async {
    final shouldDelete = await showDialog<bool>(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('Urunu Sil'),
        content: Text('${product.sku} - ${product.name} urunu silinsin mi?'),
        actions: [
          TextButton(
            onPressed: () => Navigator.of(context).pop(false),
            child: const Text('Vazgec'),
          ),
          FilledButton(
            onPressed: () => Navigator.of(context).pop(true),
            child: const Text('Sil'),
          ),
        ],
      ),
    );

    if (shouldDelete != true) {
      return;
    }

    try {
      await _api.deleteProduct(product.id);
      _showMessage('Urun kaydi silindi.');
      await _loadProducts();
    } on ApiException catch (error) {
      _showMessage(error.message);
    }
  }

  void _showMessage(String message) {
    if (!mounted) {
      return;
    }
    ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text(message)));
  }

  @override
  Widget build(BuildContext context) {
    return Card(
      elevation: 0,
      child: Padding(
        padding: const EdgeInsets.all(14),
        child: Column(
          children: [
            Row(
              children: [
                Text(
                  _editingProductId == null ? 'Urun Katalogu' : 'Urun Duzenle #$_editingProductId',
                  style: Theme.of(context).textTheme.titleMedium?.copyWith(
                        fontWeight: FontWeight.w700,
                      ),
                ),
                const Spacer(),
                IconButton(
                  tooltip: 'Yenile',
                  onPressed: _loadProducts,
                  icon: const Icon(Icons.refresh_rounded),
                ),
              ],
            ),
            const SizedBox(height: 8),
            Wrap(
              spacing: 8,
              runSpacing: 8,
              children: [
                _sizedInput(_skuController, 'SKU', 140),
                _sizedInput(_nameController, 'Urun Adi', 220),
                _sizedInput(_categoryController, 'Kategori', 170),
                _sizedInput(_uomController, 'UOM', 90),
                _sizedInput(
                  _safetyStockController,
                  'Safety',
                  90,
                  keyboardType: TextInputType.number,
                ),
                _sizedInput(
                  _reorderPointController,
                  'Reorder',
                  90,
                  keyboardType: TextInputType.number,
                ),
                SizedBox(
                  width: 170,
                  child: DropdownButtonFormField<ProductType>(
                    value: _productType,
                    decoration: const InputDecoration(labelText: 'Urun Tipi'),
                    items: ProductType.values
                        .map(
                          (type) => DropdownMenuItem(
                            value: type,
                            child: Text(type.label),
                          ),
                        )
                        .toList(),
                    onChanged: (value) {
                      if (value == null) {
                        return;
                      }
                      setState(() {
                        _productType = value;
                      });
                    },
                  ),
                ),
                SizedBox(
                  width: 150,
                  child: SwitchListTile(
                    dense: true,
                    title: const Text('Stok Takibi'),
                    value: _isStockTracked,
                    onChanged: (value) {
                      setState(() {
                        _isStockTracked = value;
                      });
                    },
                  ),
                ),
              ],
            ),
            const SizedBox(height: 10),
            Row(
              children: [
                FilledButton.icon(
                  onPressed: _isSubmitting ? null : _submitProduct,
                  icon: _isSubmitting
                      ? const SizedBox(
                          width: 16,
                          height: 16,
                          child: CircularProgressIndicator(strokeWidth: 2),
                        )
                      : const Icon(Icons.save_rounded),
                  label: Text(_editingProductId == null ? 'Urun Ekle' : 'Urun Guncelle'),
                ),
                const SizedBox(width: 8),
                OutlinedButton.icon(
                  onPressed: _clearForm,
                  icon: const Icon(Icons.clear_rounded),
                  label: const Text('Temizle'),
                ),
              ],
            ),
            const SizedBox(height: 10),
            Expanded(child: _buildProductList()),
          ],
        ),
      ),
    );
  }

  Widget _sizedInput(
    TextEditingController controller,
    String label,
    double width, {
    TextInputType keyboardType = TextInputType.text,
  }) {
    return SizedBox(
      width: width,
      child: TextField(
        controller: controller,
        keyboardType: keyboardType,
        decoration: InputDecoration(labelText: label),
      ),
    );
  }

  Widget _buildProductList() {
    if (_isLoading) {
      return const Center(child: CircularProgressIndicator());
    }
    if (_loadError != null) {
      return Center(
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            Text(_loadError!, textAlign: TextAlign.center),
            const SizedBox(height: 8),
            FilledButton(
              onPressed: _loadProducts,
              child: const Text('Tekrar Dene'),
            ),
          ],
        ),
      );
    }
    if (_products.isEmpty) {
      return const Center(child: Text('Kayitli urun bulunmadi.'));
    }
    return ListView.separated(
      itemCount: _products.length,
      separatorBuilder: (_, __) => const Divider(height: 1),
      itemBuilder: (context, index) {
        final product = _products[index];
        return ListTile(
          leading: CircleAvatar(child: Text(product.id.toString())),
          title: Text('${product.sku} • ${product.name}'),
          subtitle: Text(
            '${product.category} • ${product.productType.label} • ${product.defaultUom} • RP ${product.reorderPoint}',
          ),
          trailing: Row(
            mainAxisSize: MainAxisSize.min,
            children: [
              IconButton(
                onPressed: () => _startEditing(product),
                icon: const Icon(Icons.edit_rounded),
              ),
              IconButton(
                onPressed: () => _deleteProduct(product),
                icon: const Icon(Icons.delete_outline_rounded),
              ),
            ],
          ),
        );
      },
    );
  }
}

class _BomBuilderPanel extends StatefulWidget {
  const _BomBuilderPanel({required this.project});

  final ProjectWorkspace project;

  @override
  State<_BomBuilderPanel> createState() => _BomBuilderPanelState();
}

class _BomBuilderPanelState extends State<_BomBuilderPanel> {
  final TextEditingController _parentSkuController = TextEditingController();
  final TextEditingController _revisionController = TextEditingController(text: 'R1');
  final TextEditingController _partCodeController = TextEditingController();
  final TextEditingController _partNameController = TextEditingController();
  final TextEditingController _qtyController = TextEditingController(text: '1');
  final TextEditingController _uomController = TextEditingController(text: 'EA');

  final Map<int, List<_BomLineDraft>> _projectLines = <int, List<_BomLineDraft>>{};

  List<_BomLineDraft> _linesForProject(int projectId) {
    return _projectLines.putIfAbsent(projectId, () {
      return <_BomLineDraft>[
        _BomLineDraft(
          partCode: 'RM-${widget.project.code}-ALU',
          partName: 'Aluminyum Levha',
          quantity: 2,
          uom: 'EA',
        ),
        _BomLineDraft(
          partCode: 'RM-${widget.project.code}-SCR',
          partName: 'Vida Seti',
          quantity: 8,
          uom: 'EA',
        ),
      ];
    });
  }

  @override
  void initState() {
    super.initState();
    _parentSkuController.text = '${widget.project.code}-FG-001';
  }

  @override
  void didUpdateWidget(covariant _BomBuilderPanel oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (oldWidget.project.id != widget.project.id) {
      _parentSkuController.text = '${widget.project.code}-FG-001';
    }
  }

  @override
  void dispose() {
    _parentSkuController.dispose();
    _revisionController.dispose();
    _partCodeController.dispose();
    _partNameController.dispose();
    _qtyController.dispose();
    _uomController.dispose();
    super.dispose();
  }

  void _addLine() {
    final partCode = _partCodeController.text.trim();
    final partName = _partNameController.text.trim();
    final qty = int.tryParse(_qtyController.text.trim());
    final uom = _uomController.text.trim();
    if (partCode.isEmpty || partName.isEmpty || qty == null || qty <= 0 || uom.isEmpty) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('BOM satiri icin gecerli parca bilgileri girin.')),
      );
      return;
    }
    setState(() {
      _linesForProject(widget.project.id)
          .add(_BomLineDraft(partCode: partCode, partName: partName, quantity: qty, uom: uom));
      _partCodeController.clear();
      _partNameController.clear();
      _qtyController.text = '1';
      _uomController.text = 'EA';
    });
  }

  @override
  Widget build(BuildContext context) {
    final lines = _linesForProject(widget.project.id);
    final totalQty = lines.fold<int>(0, (sum, line) => sum + line.quantity);
    return ListView(
      children: [
        _PanelCard(
          title: '${widget.project.code} / BOM Olusturma ve Parca Listesi',
          child: Column(
            children: [
              Wrap(
                spacing: 8,
                runSpacing: 8,
                children: [
                  _input(_parentSkuController, 'Urun SKU', 150),
                  _input(_revisionController, 'Revizyon', 120),
                  _input(_partCodeController, 'Parca Kodu', 160),
                  _input(_partNameController, 'Parca Adi', 220),
                  _input(
                    _qtyController,
                    'Miktar',
                    90,
                    keyboardType: TextInputType.number,
                  ),
                  _input(_uomController, 'UOM', 90),
                  Padding(
                    padding: const EdgeInsets.only(top: 8),
                    child: FilledButton.icon(
                      onPressed: _addLine,
                      icon: const Icon(Icons.add_circle_outline_rounded),
                      label: const Text('Satir Ekle'),
                    ),
                  ),
                ],
              ),
              const SizedBox(height: 12),
              Align(
                alignment: Alignment.centerLeft,
                child: Text(
                  'Toplam Parca Satiri: ${lines.length} • Toplam Miktar: $totalQty',
                  style: const TextStyle(fontWeight: FontWeight.w600),
                ),
              ),
            ],
          ),
        ),
        const SizedBox(height: 12),
        _PanelCard(
          title: 'Parca Listesi',
          child: lines.isEmpty
              ? const Text('BOM satiri bulunmuyor.')
              : Column(
                  children: lines
                      .asMap()
                      .entries
                      .map(
                        (entry) => Padding(
                          padding: const EdgeInsets.symmetric(vertical: 4),
                          child: Row(
                            children: [
                              Expanded(flex: 2, child: Text(entry.value.partCode)),
                              Expanded(flex: 4, child: Text(entry.value.partName)),
                              Expanded(
                                flex: 2,
                                child: Text('${entry.value.quantity} ${entry.value.uom}'),
                              ),
                              IconButton(
                                tooltip: 'Satiri Sil',
                                onPressed: () {
                                  setState(() {
                                    lines.removeAt(entry.key);
                                  });
                                },
                                icon: const Icon(Icons.delete_outline_rounded),
                              ),
                            ],
                          ),
                        ),
                      )
                      .toList(),
                ),
        ),
      ],
    );
  }

  Widget _input(
    TextEditingController controller,
    String label,
    double width, {
    TextInputType keyboardType = TextInputType.text,
  }) {
    return SizedBox(
      width: width,
      child: TextField(
        controller: controller,
        keyboardType: keyboardType,
        decoration: InputDecoration(labelText: label),
      ),
    );
  }
}

class StockManagementPanel extends StatefulWidget {
  const StockManagementPanel({super.key, required this.session, required this.project});

  final AppSession session;
  final ProjectWorkspace project;

  @override
  State<StockManagementPanel> createState() => _StockManagementPanelState();
}

class _StockManagementPanelState extends State<StockManagementPanel> {
  late PersonApiClient _api;

  final TextEditingController _productIdController = TextEditingController(text: '1');
  final TextEditingController _warehouseController = TextEditingController(text: 'MAIN');
  final TextEditingController _quantityController = TextEditingController(text: '1');
  final TextEditingController _reasonController = TextEditingController(text: 'manual_adjustment');

  StockMovementType _movementType = StockMovementType.receipt;
  bool _loadingBalance = false;
  bool _posting = false;
  StockBalanceItem? _balance;
  String? _errorText;
  final Map<int, List<String>> _movementHistoryByProject = <int, List<String>>{};

  List<String> _historyForProject(int projectId) {
    return _movementHistoryByProject.putIfAbsent(projectId, () => <String>[]);
  }

  @override
  void initState() {
    super.initState();
    _api = PersonApiClient(
      baseUrl: widget.session.baseUrl,
      bearerToken: widget.session.token,
    );
    _reasonController.text = '${widget.project.code}_movement';
  }

  @override
  void didUpdateWidget(covariant StockManagementPanel oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (oldWidget.session.baseUrl != widget.session.baseUrl ||
        oldWidget.session.token != widget.session.token) {
      _api.dispose();
      _api = PersonApiClient(
        baseUrl: widget.session.baseUrl,
        bearerToken: widget.session.token,
      );
    }
    if (oldWidget.project.id != widget.project.id) {
      _reasonController.text = '${widget.project.code}_movement';
      _balance = null;
      _errorText = null;
    }
  }

  @override
  void dispose() {
    _api.dispose();
    _productIdController.dispose();
    _warehouseController.dispose();
    _quantityController.dispose();
    _reasonController.dispose();
    super.dispose();
  }

  Future<void> _loadBalance() async {
    final productId = int.tryParse(_productIdController.text.trim());
    final warehouse = _warehouseController.text.trim();
    if (productId == null || productId <= 0 || warehouse.isEmpty) {
      _showMessage('Gecerli productId ve warehouse girin.');
      return;
    }

    setState(() {
      _loadingBalance = true;
      _errorText = null;
    });

    try {
      final balance = await _api.getStockBalance(
        productId: productId,
        warehouseCode: warehouse,
      );
      if (!mounted) {
        return;
      }
      setState(() {
        _balance = balance;
      });
    } on ApiException catch (error) {
      if (!mounted) {
        return;
      }
      setState(() {
        _errorText = error.message;
      });
    } finally {
      if (!mounted) {
        return;
      }
      setState(() {
        _loadingBalance = false;
      });
    }
  }

  Future<void> _postMovement() async {
    if (_posting) {
      return;
    }

    final productId = int.tryParse(_productIdController.text.trim());
    final warehouse = _warehouseController.text.trim();
    final qty = int.tryParse(_quantityController.text.trim());
    final reason = _reasonController.text.trim();
    if (productId == null || productId <= 0 || warehouse.isEmpty || qty == null || qty <= 0 || reason.isEmpty) {
      _showMessage('Hareket bilgilerini gecerli girin.');
      return;
    }

    setState(() {
      _posting = true;
      _errorText = null;
    });

    try {
      final input = StockMovementInput(
        productId: productId,
        warehouseCode: warehouse,
        movementType: _movementType,
        quantity: qty,
        reason: reason,
      );
      final balance = await _api.postStockMovement(input);
      if (!mounted) {
        return;
      }
      setState(() {
        _balance = balance;
        _historyForProject(widget.project.id).insert(
          0,
          '${widget.project.code} • ${_movementType.label} • P${input.productId} • Q${input.quantity} • ${input.warehouseCode}',
        );
      });
      _showMessage('Stok hareketi kaydedildi.');
    } on ApiException catch (error) {
      if (!mounted) {
        return;
      }
      setState(() {
        _errorText = error.message;
      });
      _showMessage(error.message);
    } finally {
      if (!mounted) {
        return;
      }
      setState(() {
        _posting = false;
      });
    }
  }

  void _showMessage(String message) {
    if (!mounted) {
      return;
    }
    ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text(message)));
  }

  @override
  Widget build(BuildContext context) {
    return ListView(
      children: [
        _PanelCard(
          title: '${widget.project.code} / Depo Stok Yonetimi',
          child: Column(
            children: [
              Wrap(
                spacing: 8,
                runSpacing: 8,
                children: [
                  _input(
                    _productIdController,
                    'Product ID',
                    110,
                    keyboardType: TextInputType.number,
                  ),
                  _input(_warehouseController, 'Warehouse', 120),
                  SizedBox(
                    width: 180,
                    child: DropdownButtonFormField<StockMovementType>(
                      value: _movementType,
                      decoration: const InputDecoration(labelText: 'Hareket Tipi'),
                      items: StockMovementType.values
                          .map(
                            (type) => DropdownMenuItem(
                              value: type,
                              child: Text(type.label),
                            ),
                          )
                          .toList(),
                      onChanged: (value) {
                        if (value == null) {
                          return;
                        }
                        setState(() {
                          _movementType = value;
                        });
                      },
                    ),
                  ),
                  _input(
                    _quantityController,
                    'Miktar',
                    90,
                    keyboardType: TextInputType.number,
                  ),
                  _input(_reasonController, 'Aciklama', 220),
                ],
              ),
              const SizedBox(height: 10),
              Row(
                children: [
                  FilledButton.icon(
                    onPressed: _posting ? null : _postMovement,
                    icon: _posting
                        ? const SizedBox(
                            width: 16,
                            height: 16,
                            child: CircularProgressIndicator(strokeWidth: 2),
                          )
                        : const Icon(Icons.swap_horiz_rounded),
                    label: const Text('Hareket Kaydet'),
                  ),
                  const SizedBox(width: 8),
                  OutlinedButton.icon(
                    onPressed: _loadingBalance ? null : _loadBalance,
                    icon: _loadingBalance
                        ? const SizedBox(
                            width: 16,
                            height: 16,
                            child: CircularProgressIndicator(strokeWidth: 2),
                          )
                        : const Icon(Icons.refresh_rounded),
                    label: const Text('Bakiye Getir'),
                  ),
                ],
              ),
            ],
          ),
        ),
        const SizedBox(height: 12),
        _PanelCard(
          title: '${widget.project.code} / Stok Bakiyesi',
          child: _balance == null
              ? Text(_errorText ?? 'Bakiye gormek icin "Bakiye Getir" secin.')
              : Column(
                  children: [
                    _ListRow('Product ID', '${_balance!.productId}', _balance!.warehouseCode),
                    _ListRow('On Hand', '${_balance!.onHand}', ''),
                    _ListRow('Reserved', '${_balance!.reserved}', ''),
                    _ListRow('Available', '${_balance!.available}', ''),
                    _ListRow(
                      'Reorder Alarmi',
                      _balance!.belowReorderPoint ? 'Aktif' : 'Normal',
                      _balance!.belowReorderPoint ? 'Kritik' : 'OK',
                    ),
                  ],
                ),
        ),
        const SizedBox(height: 12),
        _PanelCard(
          title: '${widget.project.code} / Hareket Gecmisi',
          child: _historyForProject(widget.project.id).isEmpty
              ? const Text('Bu proje icin stok hareketi kaydi bulunmuyor.')
              : Column(
                  children: _historyForProject(widget.project.id)
                      .take(8)
                      .map(
                        (line) => Padding(
                          padding: const EdgeInsets.symmetric(vertical: 4),
                          child: Row(
                            children: [
                              const Icon(Icons.history_rounded, size: 18, color: Color(0xFF475569)),
                              const SizedBox(width: 8),
                              Expanded(child: Text(line)),
                            ],
                          ),
                        ),
                      )
                      .toList(),
                ),
        ),
      ],
    );
  }

  Widget _input(
    TextEditingController controller,
    String label,
    double width, {
    TextInputType keyboardType = TextInputType.text,
  }) {
    return SizedBox(
      width: width,
      child: TextField(
        controller: controller,
        keyboardType: keyboardType,
        decoration: InputDecoration(labelText: label),
      ),
    );
  }
}

class _BomLineDraft {
  const _BomLineDraft({
    required this.partCode,
    required this.partName,
    required this.quantity,
    required this.uom,
  });

  final String partCode;
  final String partName;
  final int quantity;
  final String uom;
}

class ReportsPage extends StatelessWidget {
  const ReportsPage({super.key});

  @override
  Widget build(BuildContext context) {
    return ListView(
      padding: const EdgeInsets.all(16),
      children: [
        const _ModuleHeader(
          title: 'Raporlama ve Analitik',
          subtitle: 'Departman bazli performans trendleri ve yonetsel gorunum.',
        ),
        const SizedBox(height: 12),
        _PanelCard(
          title: 'Aylik Performans Endeksi',
          child: Row(
            crossAxisAlignment: CrossAxisAlignment.end,
            children: const [
              _BarMetric(label: 'Oca', value: 0.62),
              _BarMetric(label: 'Sub', value: 0.68),
              _BarMetric(label: 'Mar', value: 0.73),
              _BarMetric(label: 'Nis', value: 0.81),
              _BarMetric(label: 'May', value: 0.77),
            ],
          ),
        ),
        const SizedBox(height: 12),
        const _PanelCard(
          title: 'Yonetim Ozeti',
          child: Column(
            children: [
              _ListRow('Brut Kar Marji', '%24.2', '+1.1 puan'),
              _ListRow('Teslimat Dogrulugu', '%96.8', '+0.4 puan'),
              _ListRow('Personel Devir Orani', '%4.1', '-0.8 puan'),
            ],
          ),
        ),
      ],
    );
  }
}

class _SettingsDrawer extends StatelessWidget {
  const _SettingsDrawer({
    required this.session,
    required this.onLogout,
  });

  final AppSession session;
  final VoidCallback onLogout;

  @override
  Widget build(BuildContext context) {
    return Drawer(
      child: SafeArea(
        child: ListView(
          padding: const EdgeInsets.all(16),
          children: [
            const CompanyLogo(size: 48),
            const SizedBox(height: 10),
            Text(
              session.companyName,
              style: Theme.of(context).textTheme.titleLarge?.copyWith(
                    fontWeight: FontWeight.w800,
                  ),
            ),
            Text(
              session.username,
              style: Theme.of(context).textTheme.bodyMedium?.copyWith(
                    color: const Color(0xFF475569),
                  ),
            ),
            const SizedBox(height: 18),
            const Text('Baglanti Bilgileri', style: TextStyle(fontWeight: FontWeight.w700)),
            const SizedBox(height: 8),
            Text('API: ${session.baseUrl}'),
            Text('Token: ${session.token?.isNotEmpty == true ? "Aktif" : "Yok"}'),
            const SizedBox(height: 18),
            const Text('Operasyon', style: TextStyle(fontWeight: FontWeight.w700)),
            const SizedBox(height: 8),
            const Text('• /health ve /ready endpointleri canli.\n• /metrics endpointi token ile korunur.'),
            const SizedBox(height: 24),
            FilledButton.icon(
              onPressed: () {
                Navigator.of(context).pop();
                onLogout();
              },
              icon: const Icon(Icons.logout_rounded),
              label: const Text('Guvenli Cikis'),
            ),
          ],
        ),
      ),
    );
  }
}

class CompanyLogo extends StatelessWidget {
  const CompanyLogo({super.key, required this.size});

  final double size;

  @override
  Widget build(BuildContext context) {
    return Container(
      width: size,
      height: size,
      decoration: BoxDecoration(
        gradient: const LinearGradient(
          colors: [_brandPrimary, _brandSecondary],
          begin: Alignment.topLeft,
          end: Alignment.bottomRight,
        ),
        borderRadius: BorderRadius.circular(size * 0.26),
      ),
      child: Center(
        child: Text(
          'N',
          style: TextStyle(
            color: Colors.white,
            fontSize: size * 0.56,
            fontWeight: FontWeight.w800,
          ),
        ),
      ),
    );
  }
}

class _ModuleMeta {
  const _ModuleMeta(this.title, this.icon);
  final String title;
  final IconData icon;
}

class _FeatureChip extends StatelessWidget {
  const _FeatureChip(this.text);
  final String text;

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 7),
      decoration: BoxDecoration(
        color: const Color(0x1FFFFFFF),
        borderRadius: BorderRadius.circular(30),
      ),
      child: Text(text, style: const TextStyle(color: Colors.white)),
    );
  }
}

class _ModuleHeader extends StatelessWidget {
  const _ModuleHeader({
    required this.title,
    required this.subtitle,
  });

  final String title;
  final String subtitle;

  @override
  Widget build(BuildContext context) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Text(
          title,
          style: Theme.of(context).textTheme.headlineSmall?.copyWith(
                fontWeight: FontWeight.w800,
              ),
        ),
        const SizedBox(height: 4),
        Text(
          subtitle,
          style: Theme.of(context).textTheme.bodyMedium?.copyWith(
                color: const Color(0xFF475569),
              ),
        ),
      ],
    );
  }
}

class _KpiCard extends StatelessWidget {
  const _KpiCard({
    required this.title,
    required this.value,
    required this.trend,
  });

  final String title;
  final String value;
  final String trend;

  @override
  Widget build(BuildContext context) {
    return Container(
      width: 220,
      padding: const EdgeInsets.all(14),
      decoration: BoxDecoration(
        color: Colors.white,
        borderRadius: BorderRadius.circular(16),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Text(title, style: const TextStyle(color: Color(0xFF475569))),
          const SizedBox(height: 8),
          Text(
            value,
            style: const TextStyle(fontSize: 24, fontWeight: FontWeight.w800),
          ),
          const SizedBox(height: 4),
          Text(trend, style: const TextStyle(color: _brandPrimary)),
        ],
      ),
    );
  }
}

class _MiniStat extends StatelessWidget {
  const _MiniStat({required this.label, required this.value});
  final String label;
  final String value;

  @override
  Widget build(BuildContext context) {
    return Container(
      width: 180,
      padding: const EdgeInsets.all(12),
      decoration: BoxDecoration(
        color: Colors.white,
        borderRadius: BorderRadius.circular(12),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Text(label, style: const TextStyle(color: Color(0xFF475569))),
          const SizedBox(height: 6),
          Text(value, style: const TextStyle(fontSize: 20, fontWeight: FontWeight.w700)),
        ],
      ),
    );
  }
}

class _PanelCard extends StatelessWidget {
  const _PanelCard({required this.title, required this.child});
  final String title;
  final Widget child;

  @override
  Widget build(BuildContext context) {
    return Container(
      width: double.infinity,
      padding: const EdgeInsets.all(14),
      decoration: BoxDecoration(
        color: Colors.white,
        borderRadius: BorderRadius.circular(16),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Text(
            title,
            style: Theme.of(context).textTheme.titleMedium?.copyWith(
                  fontWeight: FontWeight.w700,
                ),
          ),
          const SizedBox(height: 10),
          child,
        ],
      ),
    );
  }
}

class _ListRow extends StatelessWidget {
  const _ListRow(this.left, this.middle, this.right);
  final String left;
  final String middle;
  final String right;

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 6),
      child: Row(
        children: [
          Expanded(flex: 4, child: Text(left)),
          Expanded(flex: 2, child: Text(middle)),
          Expanded(
            flex: 2,
            child: Align(
              alignment: Alignment.centerRight,
              child: Text(
                right,
                style: const TextStyle(color: Color(0xFF0F766E), fontWeight: FontWeight.w600),
              ),
            ),
          ),
        ],
      ),
    );
  }
}

class _ActionBadge extends StatelessWidget {
  const _ActionBadge(this.label);
  final String label;

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 8),
      decoration: BoxDecoration(
        borderRadius: BorderRadius.circular(10),
        color: const Color(0xFFE2F3F5),
      ),
      child: Text(
        label,
        style: const TextStyle(
          color: Color(0xFF0F4F4A),
          fontWeight: FontWeight.w600,
        ),
      ),
    );
  }
}

class _ProgressRow extends StatelessWidget {
  const _ProgressRow(this.label, this.value);
  final String label;
  final double value;

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 6),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Text(label),
          const SizedBox(height: 4),
          LinearProgressIndicator(value: value, minHeight: 8),
        ],
      ),
    );
  }
}

class _BarMetric extends StatelessWidget {
  const _BarMetric({required this.label, required this.value});
  final String label;
  final double value;

  @override
  Widget build(BuildContext context) {
    return Expanded(
      child: Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          Container(
            height: 130 * value,
            margin: const EdgeInsets.symmetric(horizontal: 6),
            decoration: BoxDecoration(
              color: _brandSecondary,
              borderRadius: BorderRadius.circular(10),
            ),
          ),
          const SizedBox(height: 8),
          Text(label),
        ],
      ),
    );
  }
}

class ProjectWorkspace {
  const ProjectWorkspace({
    required this.id,
    required this.code,
    required this.name,
    required this.customer,
    required this.plannedBudget,
    required this.actualBudget,
  });

  final int id;
  final String code;
  final String name;
  final String customer;
  final double plannedBudget;
  final double actualBudget;

  double get variance => plannedBudget - actualBudget;

  ProjectWorkspace copyWith({
    int? id,
    String? code,
    String? name,
    String? customer,
    double? plannedBudget,
    double? actualBudget,
  }) {
    return ProjectWorkspace(
      id: id ?? this.id,
      code: code ?? this.code,
      name: name ?? this.name,
      customer: customer ?? this.customer,
      plannedBudget: plannedBudget ?? this.plannedBudget,
      actualBudget: actualBudget ?? this.actualBudget,
    );
  }
}

class AppSession {
  const AppSession({
    required this.companyName,
    required this.username,
    required this.baseUrl,
    required this.token,
  });

  final String companyName;
  final String username;
  final String baseUrl;
  final String? token;
}

enum ProductType {
  finished,
  semi,
  raw,
}

extension ProductTypeX on ProductType {
  String get label {
    switch (this) {
      case ProductType.finished:
        return 'Finished';
      case ProductType.semi:
        return 'Semi';
      case ProductType.raw:
        return 'Raw';
    }
  }

  String get apiValue {
    switch (this) {
      case ProductType.finished:
        return 'PRODUCT_TYPE_FINISHED';
      case ProductType.semi:
        return 'PRODUCT_TYPE_SEMI';
      case ProductType.raw:
        return 'PRODUCT_TYPE_RAW';
    }
  }

  static ProductType fromApi(dynamic value) {
    final raw = value is String ? value : '';
    switch (raw) {
      case 'PRODUCT_TYPE_SEMI':
        return ProductType.semi;
      case 'PRODUCT_TYPE_RAW':
        return ProductType.raw;
      case 'PRODUCT_TYPE_FINISHED':
      default:
        return ProductType.finished;
    }
  }
}

enum StockMovementType {
  receipt,
  issue,
  adjustment,
}

extension StockMovementTypeX on StockMovementType {
  String get label {
    switch (this) {
      case StockMovementType.receipt:
        return 'Receipt';
      case StockMovementType.issue:
        return 'Issue';
      case StockMovementType.adjustment:
        return 'Adjustment';
    }
  }

  String get apiValue {
    switch (this) {
      case StockMovementType.receipt:
        return 'STOCK_MOVEMENT_TYPE_RECEIPT';
      case StockMovementType.issue:
        return 'STOCK_MOVEMENT_TYPE_ISSUE';
      case StockMovementType.adjustment:
        return 'STOCK_MOVEMENT_TYPE_ADJUSTMENT';
    }
  }
}

class ProductItem {
  const ProductItem({
    required this.id,
    required this.sku,
    required this.name,
    required this.category,
    required this.defaultUom,
    required this.productType,
    required this.isStockTracked,
    required this.safetyStock,
    required this.reorderPoint,
  });

  final int id;
  final String sku;
  final String name;
  final String category;
  final String defaultUom;
  final ProductType productType;
  final bool isStockTracked;
  final int safetyStock;
  final int reorderPoint;

  factory ProductItem.fromJson(Map<String, dynamic> json) {
    return ProductItem(
      id: Person._readInt(json['id']),
      sku: Person._readString(json['sku']),
      name: Person._readString(json['name']),
      category: Person._readString(json['category']),
      defaultUom: Person._readString(json['defaultUom']),
      productType: ProductTypeX.fromApi(json['productType']),
      isStockTracked: json['isStockTracked'] == true,
      safetyStock: Person._readInt(json['safetyStock']),
      reorderPoint: Person._readInt(json['reorderPoint']),
    );
  }
}

class ProductInput {
  const ProductInput({
    required this.sku,
    required this.name,
    required this.category,
    required this.defaultUom,
    required this.productType,
    required this.isStockTracked,
    required this.safetyStock,
    required this.reorderPoint,
  });

  final String sku;
  final String name;
  final String category;
  final String defaultUom;
  final ProductType productType;
  final bool isStockTracked;
  final int safetyStock;
  final int reorderPoint;

  Map<String, dynamic> toJson() {
    return {
      'sku': sku,
      'name': name,
      'category': category,
      'defaultUom': defaultUom,
      'productType': productType.apiValue,
      'isStockTracked': isStockTracked,
      'safetyStock': safetyStock,
      'reorderPoint': reorderPoint,
    };
  }
}

class StockBalanceItem {
  const StockBalanceItem({
    required this.productId,
    required this.warehouseCode,
    required this.onHand,
    required this.reserved,
    required this.available,
    required this.belowReorderPoint,
  });

  final int productId;
  final String warehouseCode;
  final int onHand;
  final int reserved;
  final int available;
  final bool belowReorderPoint;

  factory StockBalanceItem.fromJson(Map<String, dynamic> json) {
    return StockBalanceItem(
      productId: Person._readInt(json['productId']),
      warehouseCode: Person._readString(json['warehouseCode']),
      onHand: Person._readInt(json['onHand']),
      reserved: Person._readInt(json['reserved']),
      available: Person._readInt(json['available']),
      belowReorderPoint: json['belowReorderPoint'] == true,
    );
  }
}

class StockMovementInput {
  const StockMovementInput({
    required this.productId,
    required this.warehouseCode,
    required this.movementType,
    required this.quantity,
    required this.reason,
  });

  final int productId;
  final String warehouseCode;
  final StockMovementType movementType;
  final int quantity;
  final String reason;

  Map<String, dynamic> toJson() {
    return {
      'productId': productId,
      'warehouseCode': warehouseCode,
      'movementType': movementType.apiValue,
      'quantity': quantity,
      'reason': reason,
    };
  }
}

class AuthStatus {
  const AuthStatus({
    required this.tokenRequired,
    required this.rawStatus,
  });

  final bool tokenRequired;
  final String rawStatus;
}

class PersonApiClient {
  PersonApiClient({
    required String baseUrl,
    this.bearerToken,
    http.Client? client,
  })  : baseUrl = _normalizeBaseUrl(baseUrl),
        _client = client ?? http.Client();

  final String baseUrl;
  final String? bearerToken;
  final http.Client _client;

  Future<void> checkHealth() async {
    final payload = await _requestJson('GET', '/health');
    final status = payload['status'];
    if (status is String && status.isNotEmpty) {
      return;
    }
    throw const ApiException('Saglik kontrolu beklenen formatta degil.');
  }

  Future<AuthStatus> getAuthStatus() async {
    final payload = await _requestJson('GET', '/auth/status');
    final status = Person._readString(payload['status']);
    return AuthStatus(
      tokenRequired: status == 'token_required',
      rawStatus: status,
    );
  }

  Future<void> verifyAccess(AuthStatus authStatus) async {
    if (authStatus.tokenRequired) {
      await _requestJson('GET', '/auth/verify');
      return;
    }
    await checkHealth();
  }

  Future<List<Person>> listPersons({
    int page = 1,
    int pageSize = 20,
    String query = '',
  }) async {
    final encodedQ = Uri.encodeQueryComponent(query);
    final payload = await _requestJson(
      'GET',
      '/persons?page=$page&pageSize=$pageSize&q=$encodedQ',
    );
    final personsJson = payload['persons'];
    if (personsJson is! List) {
      return const <Person>[];
    }
    return personsJson
        .whereType<Map>()
        .map((item) => Person.fromJson(item.cast<String, dynamic>()))
        .toList();
  }

  Future<Person> createPerson(PersonInput input) async {
    final payload = await _requestJson(
      'POST',
      '/persons',
      body: {'person': input.toJson()},
    );
    return _extractPerson(payload);
  }

  Future<Person> updatePerson(int id, PersonInput input) async {
    final payload = await _requestJson(
      'PUT',
      '/persons/$id',
      body: {'person': input.toJson()},
    );
    return _extractPerson(payload);
  }

  Future<void> deletePerson(int id) async {
    await _requestJson('DELETE', '/persons/$id');
  }

  Future<List<ProductItem>> listProducts({
    int page = 1,
    int pageSize = 20,
    String query = '',
  }) async {
    final encodedQ = Uri.encodeQueryComponent(query);
    final payload = await _requestJson(
      'GET',
      '/products?page=$page&pageSize=$pageSize&q=$encodedQ',
    );
    final productsJson = payload['products'];
    if (productsJson is! List) {
      return const <ProductItem>[];
    }
    return productsJson
        .whereType<Map>()
        .map((item) => ProductItem.fromJson(item.cast<String, dynamic>()))
        .toList();
  }

  Future<ProductItem> createProduct(ProductInput input) async {
    final payload = await _requestJson(
      'POST',
      '/products',
      body: {'product': input.toJson()},
    );
    return _extractProduct(payload);
  }

  Future<ProductItem> updateProduct(int id, ProductInput input) async {
    final payload = await _requestJson(
      'PUT',
      '/products/$id',
      body: {'product': input.toJson()},
    );
    return _extractProduct(payload);
  }

  Future<void> deleteProduct(int id) async {
    await _requestJson('DELETE', '/products/$id');
  }

  Future<StockBalanceItem> getStockBalance({
    required int productId,
    required String warehouseCode,
  }) async {
    final encodedWarehouse = Uri.encodeQueryComponent(warehouseCode);
    final payload = await _requestJson(
      'GET',
      '/stock?productId=$productId&warehouseCode=$encodedWarehouse',
    );
    return _extractStockBalance(payload);
  }

  Future<StockBalanceItem> postStockMovement(StockMovementInput input) async {
    final payload = await _requestJson(
      'POST',
      '/stock/movements',
      body: {'movement': input.toJson()},
    );
    return _extractStockBalance(payload);
  }

  void dispose() {
    _client.close();
  }

  static String _normalizeBaseUrl(String rawBaseUrl) {
    final trimmed = rawBaseUrl.trim();
    if (trimmed.endsWith('/')) {
      return trimmed.substring(0, trimmed.length - 1);
    }
    return trimmed;
  }

  Future<Map<String, dynamic>> _requestJson(
    String method,
    String path, {
    Map<String, dynamic>? body,
  }) async {
    final request = http.Request(method, Uri.parse('$baseUrl$path'));
    request.headers['Accept'] = 'application/json';
    final token = bearerToken?.trim();
    if (token != null && token.isNotEmpty) {
      request.headers['Authorization'] = 'Bearer $token';
    }
    if (body != null) {
      request.headers['Content-Type'] = 'application/json';
      request.body = jsonEncode(body);
    }

    try {
      final streamedResponse = await _client.send(request);
      final response = await http.Response.fromStream(streamedResponse);
      final payload = _decodeResponseBodyAsMap(response);
      if (response.statusCode >= 400) {
        throw ApiException(_extractErrorMessage(payload, response.statusCode));
      }
      return payload;
    } catch (error) {
      if (error is ApiException) {
        rethrow;
      }
      throw ApiException('Istek basarisiz: $error');
    }
  }

  Map<String, dynamic> _decodeResponseBodyAsMap(http.Response response) {
    final body = response.body;
    if (body.isEmpty) {
      return <String, dynamic>{};
    }

    final contentType = (response.headers['content-type'] ?? '').toLowerCase();
    if (!contentType.contains('application/json')) {
      throw ApiException(
        'API JSON donmedi (HTTP ${response.statusCode}). Yanit turu: ${response.headers['content-type'] ?? "unknown"}',
      );
    }

    try {
      final dynamic decoded = jsonDecode(body);
      if (decoded is Map<String, dynamic>) {
        return decoded;
      }
      if (decoded is Map) {
        return decoded.cast<String, dynamic>();
      }
      throw const ApiException('Beklenmeyen yanit formati.');
    } on FormatException {
      throw const ApiException('Gecersiz JSON yaniti alindi.');
    }
  }

  String _extractErrorMessage(Map<String, dynamic> payload, int statusCode) {
    final errorValue = payload['error'];
    if (errorValue is Map) {
      final message = errorValue['message'];
      if (message is String && message.isNotEmpty) {
        return message;
      }
    }
    return 'Istek hata kodu: $statusCode';
  }

  Person _extractPerson(Map<String, dynamic> payload) {
    final personValue = payload['person'];
    if (personValue is Map) {
      return Person.fromJson(personValue.cast<String, dynamic>());
    }
    throw const ApiException('Yanitta personel verisi bulunamadi.');
  }

  ProductItem _extractProduct(Map<String, dynamic> payload) {
    final productValue = payload['product'];
    if (productValue is Map) {
      return ProductItem.fromJson(productValue.cast<String, dynamic>());
    }
    throw const ApiException('Yanitta urun verisi bulunamadi.');
  }

  StockBalanceItem _extractStockBalance(Map<String, dynamic> payload) {
    final balanceValue = payload['balance'];
    if (balanceValue is Map) {
      return StockBalanceItem.fromJson(balanceValue.cast<String, dynamic>());
    }
    throw const ApiException('Yanitta stok bakiyesi bulunamadi.');
  }
}

class Person {
  const Person({
    required this.id,
    required this.firstName,
    required this.lastName,
    required this.email,
    required this.age,
  });

  final int id;
  final String firstName;
  final String lastName;
  final String email;
  final int age;

  factory Person.fromJson(Map<String, dynamic> json) {
    return Person(
      id: _readInt(json['id']),
      firstName: _readString(json['firstName']),
      lastName: _readString(json['lastName']),
      email: _readString(json['email']),
      age: _readInt(json['age']),
    );
  }

  static int _readInt(dynamic value) {
    if (value is int) {
      return value;
    }
    if (value is String) {
      return int.tryParse(value) ?? 0;
    }
    return 0;
  }

  static String _readString(dynamic value) {
    if (value is String) {
      return value;
    }
    return '';
  }
}

class PersonInput {
  const PersonInput({
    required this.firstName,
    required this.lastName,
    required this.email,
    required this.age,
  });

  final String firstName;
  final String lastName;
  final String email;
  final int age;

  Map<String, dynamic> toJson() {
    return {
      'firstName': firstName,
      'lastName': lastName,
      'email': email,
      'age': age,
    };
  }
}

class ApiException implements Exception {
  const ApiException(this.message);
  final String message;

  @override
  String toString() => message;
}

String defaultApiBaseUrl() {
  if (_configuredBaseUrl.isNotEmpty) {
    return _configuredBaseUrl;
  }
  if (kIsWeb) {
    return 'http://127.0.0.1:18080';
  }
  if (defaultTargetPlatform == TargetPlatform.android) {
    return 'http://10.0.2.2:18080';
  }
  return 'http://127.0.0.1:18080';
}
