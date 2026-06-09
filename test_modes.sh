#!/bin/bash
# =============================================================================
# ft_irc - Test Suite
# Uso: ./test_modes.sh [host] [port] [password]
# =============================================================================

HOST="${1:-127.0.0.1}"
PORT="${2:-6667}"
PASS="${3:-test}"

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

PASS_COUNT=0
FAIL_COUNT=0
TOTAL=0

# =============================================================================
# capture: escribe comandos en un fichero temporal con delays entre líneas,
# lo pasa a nc con timeout y recoge la respuesta.
# =============================================================================
capture() {
    local commands="$1"
    local input_file
    local output_file
    input_file=$(mktemp)
    output_file=$(mktemp)

    # Escribir cada línea en el fichero de entrada con un sleep entre ellas
    # usando un script auxiliar que nc leerá por stdin
    (
        while IFS= read -r line; do
            [[ -z "$line" ]] && continue
            printf '%s\r\n' "$line"
            sleep 0.2
        done <<< "$commands"
        sleep 0.4
    ) > "$input_file"

    # nc lee el fichero (ya tiene los delays "dentro") y escribe la respuesta
    nc -q 1 "$HOST" "$PORT" < "$input_file" > "$output_file" 2>/dev/null

    cat "$output_file"
    rm -f "$input_file" "$output_file"
}

run_test() {
    local description="$1"
    local commands="$2"
    local expected="$3"
    local should_fail="${4:-false}"

    TOTAL=$((TOTAL + 1))
    local output
    output=$(capture "$commands")

    local found=false
    echo "$output" | grep -q "$expected" && found=true

    if [ "$should_fail" = "true" ]; then
        if [ "$found" = "false" ]; then
            echo -e "  ${GREEN}✓ PASS${NC} - $description"
            PASS_COUNT=$((PASS_COUNT + 1))
        else
            echo -e "  ${RED}✗ FAIL${NC} - $description"
            echo -e "       ${YELLOW}No debería contener:${NC} '$expected'"
            echo -e "       ${YELLOW}Output:${NC} $(echo "$output" | tail -3)"
            FAIL_COUNT=$((FAIL_COUNT + 1))
        fi
    else
        if [ "$found" = "true" ]; then
            echo -e "  ${GREEN}✓ PASS${NC} - $description"
            PASS_COUNT=$((PASS_COUNT + 1))
        else
            echo -e "  ${RED}✗ FAIL${NC} - $description"
            echo -e "       ${YELLOW}Esperado:${NC} '$expected'"
            echo -e "       ${YELLOW}Output:${NC} $(echo "$output" | tail -3)"
            FAIL_COUNT=$((FAIL_COUNT + 1))
        fi
    fi
}

reg() { printf "PASS %s\nNICK %s\nUSER %s 0 * %s" "$PASS" "$1" "$1" "$1"; }

# =============================================================================
# VERIFICAR SERVIDOR
# =============================================================================
echo ""
echo -e "${BOLD}${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${BOLD}${CYAN}   ft_irc MODE Test Suite${NC}"
echo -e "${BOLD}${CYAN}   Servidor: $HOST:$PORT  |  Pass: $PASS${NC}"
echo -e "${BOLD}${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo ""

if ! nc -z "$HOST" "$PORT" 2>/dev/null; then
    echo -e "${RED}ERROR: No se puede conectar al servidor en $HOST:$PORT${NC}"
    echo -e "${YELLOW}Asegurate de tener el servidor corriendo: ./ircserv $PORT $PASS${NC}"
    exit 1
fi
echo -e "${GREEN}Servidor detectado en $HOST:$PORT${NC}"
echo ""

# =============================================================================
# 1. REGISTRO Y AUTENTICACIÓN
# =============================================================================
echo -e "${BOLD}── 1. REGISTRO Y AUTENTICACION ──────────────────${NC}"

run_test "PASS correcto -> welcome (001)" \
    "$(reg r_ok)" "001"

run_test "PASS incorrecto -> 464" \
    "$(printf "PASS wrongpass\nNICK badguy\nUSER badguy 0 * badguy")" "464"

run_test "Comando JOIN sin registrar -> 451" \
    "$(printf "JOIN #test")" "451"

run_test "Nickname duplicado -> 433" \
    "$(printf "PASS %s\nNICK r_ok\nUSER dup 0 * dup" "$PASS")" "433"

# =============================================================================
# 2. JOIN BÁSICO
# =============================================================================
echo ""
echo -e "${BOLD}── 2. JOIN BASICO ───────────────────────────────${NC}"

run_test "JOIN crea el canal y el usuario entra" \
    "$(printf "%s\nJOIN #basictest" "$(reg jt1)")" "JOIN :#basictest"

run_test "JOIN segundo usuario en canal existente" \
    "$(printf "%s\nJOIN #basictest" "$(reg jt2)")" "JOIN :#basictest"

run_test "JOIN canal sin # lo añade automaticamente" \
    "$(printf "%s\nJOIN basictest2" "$(reg jt3)")" "JOIN :#basictest2"

run_test "JOIN muestra topic vacio (331)" \
    "$(printf "%s\nJOIN #basictest" "$(reg jt4)")" "331"

# =============================================================================
# 3. MODO +i / -i
# =============================================================================
echo ""
echo -e "${BOLD}── 3. MODO +i (Invite Only) ─────────────────────${NC}"

run_test "Activar MODE +i -> confirmacion" \
    "$(printf "%s\nJOIN #invtest\nMODE #invtest +i" "$(reg op_i1)")" "MODE #invtest +i"

run_test "Usuario sin invitacion bloqueado -> 473" \
    "$(printf "%s\nJOIN #invtest" "$(reg noinv1)")" "473"

run_test "INVITE genera RPL_INVITING (341)" \
    "$(printf "%s\nJOIN #invtest\nINVITE noinv1 #invtest" "$(reg op_i1)")" "341"

run_test "No-operador no puede +i -> 482" \
    "$(printf "%s\nJOIN #invtest\nMODE #invtest +i" "$(reg noop_i)")" "482"

run_test "Desactivar MODE -i -> confirmacion" \
    "$(printf "%s\nJOIN #invtest\nMODE #invtest -i" "$(reg op_i1)")" "MODE #invtest -i"

# =============================================================================
# 4. MODO +t / -t
# =============================================================================
echo ""
echo -e "${BOLD}── 4. MODO +t (Topic Restricted) ────────────────${NC}"

run_test "Canal nuevo tiene +t -> no-op bloqueado (482)" \
    "$(printf "%s\nJOIN #topict\nTOPIC #topict :sin permisos" "$(reg noop_t)")" "482"

run_test "Operador cambia TOPIC con +t activo" \
    "$(printf "%s\nJOIN #topict2\nTOPIC #topict2 :Tema VIP" "$(reg op_t)")" "TOPIC #topict2 :Tema VIP"

run_test "MODE -t -> confirmacion" \
    "$(printf "%s\nJOIN #topict3\nMODE #topict3 -t" "$(reg op_t2)")" "MODE #topict3 -t"

run_test "Con -t activo, no-op puede cambiar TOPIC" \
    "$(printf "%s\nJOIN #topict3\nTOPIC #topict3 :libre" "$(reg noop_t2)")" "TOPIC #topict3 :libre"

# =============================================================================
# 5. MODO +k / -k
# =============================================================================
echo ""
echo -e "${BOLD}── 5. MODO +k (Channel Key) ─────────────────────${NC}"

run_test "Activar +k secreto -> confirmacion" \
    "$(printf "%s\nJOIN #keyt\nMODE #keyt +k secreto" "$(reg op_k)")" "MODE #keyt +k secreto"

run_test "JOIN sin clave -> 475" \
    "$(printf "%s\nJOIN #keyt" "$(reg nokey)")" "475"

run_test "JOIN con clave incorrecta -> 475" \
    "$(printf "%s\nJOIN #keyt wrongkey" "$(reg wrongk)")" "475"

run_test "JOIN con clave correcta -> entra" \
    "$(printf "%s\nJOIN #keyt secreto" "$(reg goodk)")" "JOIN :#keyt"

run_test "MODE +k sin parametro -> 461" \
    "$(printf "%s\nJOIN #keyt2\nMODE #keyt2 +k" "$(reg op_k2)")" "461"

# =============================================================================
# 6. MODO +l / -l
# =============================================================================
echo ""
echo -e "${BOLD}── 6. MODO +l (User Limit) ──────────────────────${NC}"

run_test "Activar +l 2 -> confirmacion" \
    "$(printf "%s\nJOIN #limt\nMODE #limt +l 2" "$(reg op_l)")" "MODE #limt +l 2"

run_test "Segundo usuario entra (dentro del limite)" \
    "$(printf "%s\nJOIN #limt" "$(reg lim2)")" "JOIN :#limt"

run_test "Tercer usuario bloqueado -> 471" \
    "$(printf "%s\nJOIN #limt" "$(reg lim3)")" "471"

run_test "MODE +l sin numero -> 461" \
    "$(printf "%s\nJOIN #limt2\nMODE #limt2 +l" "$(reg op_l2)")" "461"

# =============================================================================
# 7. MODO +o / -o
# =============================================================================
echo ""
echo -e "${BOLD}── 7. MODO +o (Operator Privilege) ──────────────${NC}"

run_test "Dar +o a miembro del canal -> confirmacion" \
    "$(printf "%s\nJOIN #opt\nMODE #opt +o lim2" "$(reg op_o)")" "MODE #opt +o lim2"

run_test "Dar +o a usuario no en el canal -> 441" \
    "$(printf "%s\nJOIN #opt2\nMODE #opt2 +o fantasma" "$(reg op_o2)")" "441"

run_test "MODE +o sin parametro -> 461" \
    "$(printf "%s\nJOIN #opt3\nMODE #opt3 +o" "$(reg op_o3)")" "461"

run_test "No-operador intenta +o -> 482" \
    "$(printf "%s\nJOIN #opt4\nMODE #opt4 +o alguien" "$(reg noop_o)")" "482"

# =============================================================================
# 8. KICK
# =============================================================================
echo ""
echo -e "${BOLD}── 8. KICK ──────────────────────────────────────${NC}"

run_test "No-operador hace KICK -> 482" \
    "$(printf "%s\nJOIN #kickt\nKICK #kickt op_k :fuera" "$(reg noop_ki)")" "482"

run_test "KICK usuario no en canal -> 441" \
    "$(printf "%s\nJOIN #kickt2\nKICK #kickt2 fantasma :fuera" "$(reg op_ki)")" "441"

run_test "KICK sin parametros -> 461" \
    "$(printf "%s\nJOIN #kickt3\nKICK" "$(reg op_ki2)")" "461"

# =============================================================================
# 9. PRIVMSG
# =============================================================================
echo ""
echo -e "${BOLD}── 9. PRIVMSG ───────────────────────────────────${NC}"

run_test "PRIVMSG a canal inexistente -> 403" \
    "$(printf "%s\nPRIVMSG #noexiste :hola" "$(reg pv1)")" "403"

run_test "PRIVMSG a usuario inexistente -> 401" \
    "$(printf "%s\nPRIVMSG fantasmauser :hola" "$(reg pv2)")" "401"

run_test "PRIVMSG sin mensaje -> 412" \
    "$(printf "%s\nJOIN #privt\nPRIVMSG #privt" "$(reg pv3)")" "412"

# =============================================================================
# 10. MODOS INVÁLIDOS
# =============================================================================
echo ""
echo -e "${BOLD}── 10. MODOS INVALIDOS ───────────────────────────${NC}"

run_test "Modo desconocido +z -> 472" \
    "$(printf "%s\nJOIN #modet\nMODE #modet +z" "$(reg op_uk)")" "472"

run_test "MODE en canal inexistente -> 403" \
    "$(printf "%s\nMODE #noexiste +i" "$(reg op_nc)")" "403"

# =============================================================================
# RESUMEN
# =============================================================================
echo ""
echo -e "${BOLD}${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${BOLD}   RESULTADOS FINALES${NC}"
echo -e "${BOLD}${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "   Total:   ${BOLD}$TOTAL${NC} tests"
echo -e "   ${GREEN}Passed:  $PASS_COUNT${NC}"
echo -e "   ${RED}Failed:  $FAIL_COUNT${NC}"
echo ""
if [ "$FAIL_COUNT" -eq 0 ]; then
    echo -e "   ${GREEN}${BOLD}Todos los tests pasaron!${NC}"
else
    PERCENT=$(( (PASS_COUNT * 100) / TOTAL ))
    echo -e "   ${YELLOW}Puntuacion: $PERCENT%${NC}"
fi
echo ""