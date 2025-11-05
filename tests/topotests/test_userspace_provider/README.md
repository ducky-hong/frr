# Userspace Provider Integration Tests

This directory contains tests that verify the userspace provider functionality with actual zebra/BGP daemons.

## Test Files

### test_verification.py
Standalone verification without zebra. Tests:
- Code structure and API completeness
- Wire protocol format
- MAC events with ESI
- Multiple event types

**Run**: `python3 test_verification.py`

**Expected**: All tests pass (4/4)

### test_integration.py
Full integration test with zebra running. Tests:
- Enable userspace provider in zebra
- Inject events via Unix socket
- Verify events appear in zebra via vtysh
- Check EVPN state
- Verify provider statistics

**Run**: `pytest test_integration.py -v -s`

**Expected**: Tests verify events reach zebra

### demo_userspace_provider.py
Interactive demonstration showing:
- How to use the Python API
- Wire protocol format
- EVPN multihoming scenario
- Kernel vs userspace comparison

**Run**: `python3 demo_userspace_provider.py`

**Expected**: Educational output about the provider

### test_userspace_basic.py
Topotest framework integration (template for real tests)

## Configuration Files

### r1/zebra.conf
```
hostname r1
log file /tmp/r1-zebra.log
!
! Enable userspace dataplane mode
userspace-dataplane
!
debug zebra events
debug zebra kernel
```

### r1/bgpd.conf
```
hostname r1
log file /tmp/r1-bgpd.log
!
router bgp 65001
 bgp router-id 1.1.1.1
 no bgp ebgp-requires-policy
 !
 address-family l2vpn evpn
  advertise-all-vni
 exit-address-family
```

## How Integration Tests Work

### 1. Setup Phase
```python
def setup_module(module):
    tgen = Topogen(build_topo, module.__name__)
    tgen.start_topology()

    # Load configs (including userspace-dataplane)
    router.load_config(TopoRouter.RD_ZEBRA, "r1/zebra.conf")
    router.load_config(TopoRouter.RD_BGP, "r1/bgpd.conf")

    tgen.start_router()
```

### 2. Test Phase
```python
def test_inject_and_verify_interface():
    r1 = tgen.gears["r1"]

    # Create provider and inject event
    provider = UsrspaceProvider()
    provider.connect()
    provider.inject_intf_add("test0", ifindex=100, mtu=1500, hw_addr=...)
    provider.inject_intf_up(ifindex=100)

    time.sleep(2)  # Give zebra time to process

    # Verify in zebra via vtysh
    output = r1.vtysh_cmd("show interface test0")
    assert "test0" in output
```

### 3. Verification Commands (from test_evpn_mh.py)

Key vtysh commands used:

**Interface verification**:
```python
r1.vtysh_cmd("show interface brief")
r1.vtysh_cmd("show interface test0")
r1.vtysh_cmd("show interface test0 json")
```

**EVPN verification**:
```python
r1.vtysh_cmd("show evpn vni")
r1.vtysh_cmd("show evpn mac vni all")
r1.vtysh_cmd("show evpn mac vni 1000 mac aa:bb:cc:dd:ee:ff")
r1.vtysh_cmd("show evpn mac vni 1000 mac aa:bb:cc:dd:ee:ff json")
```

**EVPN ES verification (main check)**:
```python
r1.vtysh_cmd("show evpn es")
r1.vtysh_cmd("show evpn es 03:44:38:39:ff:ff:01:00:00:01 json")
r1.vtysh_cmd("show bgp l2vpn evpn es")
r1.vtysh_cmd("show bgp l2vpn evpn es json")
```

**Provider statistics**:
```python
r1.vtysh_cmd("show userspace-dataplane provider")
```

## Complete EVPN MH Test Scenario

Based on test_evpn_mh.py, here's the complete flow:

```python
def test_evpn_mh_userspace():
    provider = UsrspaceProvider()
    provider.connect()

    # 1. Create bridge
    provider.inject_intf_add("bridge", ifindex=400, is_bridge=True)
    provider.inject_intf_up(ifindex=400)

    # 2. Create VXLAN
    provider.inject_intf_add("vxlan1000", ifindex=500,
                             is_vxlan=True, vni=1000,
                             vtep_ip="192.168.100.1")
    provider.inject_intf_up(ifindex=500)

    # 3. Create bond (LAG for MH)
    provider.inject_intf_add("bond0", ifindex=600, is_bond=True,
                             hw_addr=b'\x44\x38\x39\xff\xff\x01')
    provider.inject_intf_up(ifindex=600)

    # 4. Inject MAC with ESI
    esi = b'\x03\x44\x38\x39\xff\xff\x01\x00\x00\x01'
    mac = b'\x00\x00\x00\x00\x00\x11'
    provider.inject_mac_add(ifindex=600, vni=1000, mac=mac,
                            vid=1000, is_local=True, esi=esi)

    time.sleep(2)

    # 5. Verify ES was created
    output = r1.vtysh_cmd("show bgp l2vpn evpn es json")
    es_data = json.loads(output)

    # Check ESI is present
    esi_str = "03:44:38:39:ff:ff:01:00:00:01"
    assert any(esi_str in es for es in es_data.keys())

    # Check ES type is local
    for es_esi, es_info in es_data.items():
        if esi_str in es_esi:
            assert "local" in es_info.get("type", [])
            print(f"✓ ES {esi_str} is local")

    provider.close()
```

## Expected Test Output

### test_verification.py
```
======================================================================
 USERSPACE PROVIDER VERIFICATION TESTS
======================================================================
✓ Code Structure                 PASS
✓ Wire Protocol                  PASS
✓ MAC with ESI                   PASS
✓ Multiple Events                PASS
----------------------------------------------------------------------
Total: 4/4 tests passed

✓✓✓ ALL TESTS PASSED ✓✓✓
```

### test_integration.py
```
test_integration.py::test_userspace_provider_enabled PASSED
✓ Userspace provider is enabled and running

test_integration.py::test_inject_and_verify_interface PASSED
✓ Injected interface test0
✓ Interface test0 is visible in zebra

test_integration.py::test_inject_and_verify_mac PASSED
✓ Injected MAC aabbccddeeff with ESI 03443839ffff01000001
✓ MAC has ESI: 03:44:38:39:ff:ff:01:00:00:01

test_integration.py::test_evpn_integration PASSED
✓ EVPN Ethernet Segments found:
  ESI: 03:44:38:39:ff:ff:01:00:00:01
    Type: ['local']
```

## Running the Tests

### Quick Verification (no zebra needed)
```bash
cd /home/user/frr/tests/topotests/test_userspace_provider
python3 test_verification.py
```

### Full Integration Test (requires zebra)
```bash
cd /home/user/frr/tests/topotests
sudo pytest test_userspace_provider/test_integration.py -v -s
```

### Run Demo
```bash
python3 test_userspace_provider/demo_userspace_provider.py
```

## Troubleshooting

### Socket Permission Denied
```bash
sudo chmod 666 /var/run/frr/zebra_usrspace.sock
```

### Zebra not processing events
Check zebra logs:
```bash
tail -f /tmp/r1-zebra.log
```

Enable debug:
```
vtysh -c "configure terminal" -c "debug zebra events" -c "debug zebra kernel"
```

### Events not appearing
Check provider statistics:
```
vtysh -c "show userspace-dataplane provider"
```

Should show:
- Events received > 0
- Events processed > 0
- Events dropped = 0

## Integration with Existing Tests

To convert test_evpn_mh.py to use userspace:

1. **In setup_module**, enable userspace mode:
```python
for router in ["torm11", "torm12", "torm21", "torm22"]:
    tgen.gears[router].vtysh_cmd(
        "configure terminal\nuserspace-dataplane"
    )
```

2. **Replace kernel operations** with provider injections:
```python
# OLD: config_bond(tor, "hostbond1", [bond_member], sys_mac, "bridge")
# NEW:
provider.inject_intf_add("hostbond1", ifindex=200, is_bond=True,
                         hw_addr=bytes.fromhex(sys_mac.replace(':', '')))
```

3. **Keep all existing verification** - no changes needed!
```python
# These still work the same:
output = dut.vtysh_cmd("show bgp l2vpn evpn es json")
# ... verification logic
```

## References

- Main implementation: `zebra/zebra_usrspace_provider.[ch]`
- Mock provider: `zebra/zebra_usrspace_mock.[ch]`
- Python API: `tests/lib/usrspace_provider.py`
- Documentation: `doc/developer/userspace-provider.rst`
- Example test: `tests/topotests/bgp_evpn_mh/test_evpn_mh.py`
